#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/demonware/achievement_engine.hpp"
#include "component/scheduler.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include <charconv>
#include "game/demonware/hq_marketplace.hpp"
#include <utils/hook.hpp>

namespace hq_economy
{
	namespace
	{
		const char* cell(const game::StringTable* table, int row, int column)
		{
			if (!table || !table->values || row < 0 || row >= table->rowCount || column < 0 || column >= table->columnCount) return "";
			const auto* value = table->values[row * table->columnCount + column].string;
			return value ? value : "";
		}

		bool parse_number(std::string_view value, std::uint32_t& output)
		{
			int base = 10;
			if (value.starts_with("0x") || value.starts_with("0X")) { value.remove_prefix(2); base = 16; }
			const auto parsed = std::from_chars(value.data(), value.data() + value.size(), output, base);
			return !value.empty() && parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
		}

		void print_state(const bool reload = false)
		{
			try
			{
				if (reload) demonware::hq_economy::invalidate();
				const auto data = demonware::hq_economy::snapshot();
				console::info("[HQ economy] revision %llu: %zu currencies, %zu items, %zu achievements\n",
					data.revision, data.currencies.size(), data.inventory.size(), data.achievements.size());
				for (const auto& [id, balance] : data.currencies) console::info("  currency %u = %u\n", id, balance);
				for (const auto& [key, item] : data.inventory) console::info("  item 0x%X collision %u = %u (expires %u)\n",
					item.guid, item.collision, item.quantity, item.expires);
				for (const auto& [name, entry] : data.achievements) console::info("  %s kind %d: %s %u/%u activated %llu claimed '%s'\n",
					name.c_str(), entry.kind, entry.status.c_str(), entry.progress, entry.target, entry.activation, entry.claim_transaction.c_str());
			}
			catch (const std::exception& error) { console::error("[HQ economy] %s\n", error.what()); }
		}

		void grant(const command::params& params)
		{
			std::uint32_t id{}, amount{};
			if (params.size() != 4 || !parse_number(params[2], id) || !parse_number(params[3], amount) || !amount ||
				(std::string_view{params[1]} != "currency" && std::string_view{params[1]} != "item"))
			{
				console::info("Usage: hqgrant <currency|item> <decimal or 0x id> <positive amount>\n");
				return;
			}
			const demonware::hq_economy::reward value{std::string_view{params[1]} == "item" ? "GRANT_PRODUCT" : "GRANT_CURRENCY", id, amount};
			if (demonware::hq_economy::transact([&](auto& data) { return demonware::hq_economy::grant(data, value); })) print_state();
			else console::warn("[HQ economy] grant rejected (ID, overflow, lock or save failure)\n");
		}

		void event_command(const command::params& params)
		{
			const std::string_view name = params.size() == 2 ? params[1] : "";
			if (name != "kill" && name != "headshot" && name != "payroll")
			{
				console::info("Usage: aeevent <kill|headshot|payroll> (changes local economy)\n");
				return;
			}
			static std::int64_t last{};
			const auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
			last = std::max(last + 1, timestamp);
			demonware::reward_game_events::event event{name == "payroll" ? "18" : "1", last, {}};
			if (name == "headshot") event.parameters.push_back({"6", 1});
			console::info("[HQ event] diagnostic event %.*s: %s; use aefetch user to refresh the native cache\n",
				static_cast<int>(name.size()), name.data(),
				demonware::achievement_engine::submit_event(event) ? "saved" : "failed");
		}

		void load_loot_catalog()
		{
			const auto* collections = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, "mp/collections.csv", false).stringTable;
			const auto* items = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, "mp/itemscollections.csv", false).stringTable;
			if (!collections || !items) return;
			std::vector<std::uint32_t> pool;
			for (int row = 0; row < items->rowCount; ++row)
			{
				bool known{};
				for (int c = 0; c < collections->rowCount; ++c)
					known |= std::string_view{cell(items, row, 0)} == cell(collections, c, 0);
				std::uint32_t count{};
				if (!known || !parse_number(cell(items, row, 2), count) || count > 64) continue;
				for (std::uint32_t col = 3; col < 3 + count; ++col)
				{
					std::uint32_t id{};
					if (parse_number(cell(items, row, static_cast<int>(col)), id)) pool.push_back(id);
				}
			}
			std::map<std::uint32_t, unsigned> rarities;
			for (const auto id : pool)
			{
				const auto rarity = utils::hook::invoke<int>(0x652330_g, id);
				if (rarity >= 0) rarities[id] = static_cast<unsigned>(rarity);
			}
			demonware::hq_marketplace::set_rarities(rarities);
			demonware::achievement_engine::set_loot_catalog(std::move(pool));
		}

		void load_catalog()
		{
			const auto* daily = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, "mp/dailychallengestable.csv", false).stringTable;
			const auto* definitions = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, "dw/dwgamechallenges.csv", false).stringTable;
			if (!daily || !definitions) return;
			// The UI table has no AE foreign key. Only unambiguous semantic joins are enabled.
			const std::map<std::string, std::string> joins
			{
				{"ch_daily_0", "daily_ch_kills"}, {"ch_daily_1", "daily_ch_headshots"},
				{"ch_daily_2", "daily_ch_1v1_wins"}, {"ch_daily_5", "daily_ch_commend"},
			};
			std::vector<demonware::hq_economy::achievement> catalog{};
			for (int row = 0; row < daily->rowCount; ++row)
			{
				const auto join = joins.find(cell(daily, row, 0));
				if (join == joins.end()) continue;
				for (int definition = 0; definition < definitions->rowCount; ++definition)
				{
					if (join->second != cell(definitions, definition, 1) || std::string_view{cell(definitions, definition, 2)} != "1") continue;
					demonware::hq_economy::achievement entry{};
					entry.name = join->second;
					entry.challenge_name = join->first;
					const std::string_view target{cell(daily, row, 9)};
					const auto parsed = std::from_chars(target.data(), target.data() + target.size(), entry.target);
					if (parsed.ec != std::errc{} || parsed.ptr != target.data() + target.size() || !entry.target) continue;
					// Column 10 is XP; 25 Armory Credits (currency 6) is a documented local reward,
					// pending a verified XP reward mapping.
					entry.rewards = {{"GRANT_CURRENCY", demonware::hq_economy::armory_credits, 25}};
					catalog.push_back(entry);
				}
			}
			// The copied weekly UI table is empty. These definition-backed offers use
			// explicit local targets; see Slice 2 report, not inferred retail values.
			for (const auto& [name, target] : std::map<std::string, std::uint32_t>{
				{"weekly_ch_kills", 100}, {"weekly_ch_wins", 10}, {"weekly_ch_scorestreak_calls", 25}})
			{
				for (int row = 0; row < definitions->rowCount; ++row)
				{
					if (name != cell(definitions, row, 1) || std::string_view{cell(definitions, row, 2)} != "2") continue;
					demonware::hq_economy::achievement entry{};
					entry.name = name; entry.challenge_name = name;
					entry.kind = 2; entry.target = target;
					entry.rewards = {{"GRANT_CURRENCY", demonware::hq_economy::armory_credits, 100}};
					catalog.push_back(entry);
				}
			}
			// Minimal local contract policy; prices/reward bundles are not in dwGameChallenges.
			for (int row = 0; row < definitions->rowCount; ++row)
			{
				const std::string name{cell(definitions, row, 1)};
				if (name != "contract_mp_1" && name != "contract_mp_2" && name != "contract_mp_3") continue;
				if (std::string_view{cell(definitions, row, 2)} != "4") continue;
				demonware::hq_economy::achievement entry{};
				entry.name = name;
				entry.challenge_name = name;
				entry.kind = 4;
				entry.usage_target = 3600;
				// A contract has to carry a reward. AE_GetScheduledChallenges (0x121A00) and
				// AE_GetPlayerActiveChallenges (0x121F40) only publish the Lua "reward" table when
				// the record's reward pointer is non-null, so a contract with an empty
				// successRewards array reaches the vendor with no reward at all while every daily
				// and weekly carries one. Local policy: a completed contract pays twice what its
				// Quartermaster SKU costs (25/50/75 Armory Credits, hq_marketplace::vendor_skus).
				const auto payout = name == "contract_mp_1" ? 50u : name == "contract_mp_2" ? 100u : 150u;
				entry.rewards = {{"GRANT_CURRENCY", demonware::hq_economy::armory_credits, payout}};
				catalog.push_back(entry);
			}
			// Rules live in the asset catalog, never in editable persisted progress records.
			std::map<std::string, demonware::hq_event_predicate::rule> rules;
			for (int row = 0; row < definitions->rowCount; ++row)
			{
				std::uint32_t kind{}, event{};
				if (!parse_number(cell(definitions, row, 2), kind) || kind < 1 || kind > 4 ||
					!parse_number(cell(definitions, row, 3), event) || !event) continue;
				rules.emplace(cell(definitions, row, 1), demonware::hq_event_predicate::rule{event, cell(definitions, row, 4)});
			}
			demonware::achievement_engine::set_event_rules(std::move(rules));
			demonware::achievement_engine::set_catalog(std::move(catalog));
		}
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated() || game::environment::is_zombies()) return;
			command::add("hqeconomy", [](const command::params& params) { print_state(params.size() > 1 && std::string_view{params[1]} == "reload"); });
			command::add("hqgrant", grant);
			command::add("aeevent", event_command);
			scheduler::loop(load_loot_catalog, scheduler::pipeline::main, 5s);
			scheduler::loop(load_catalog, scheduler::pipeline::main, 5s);
		}
	};
}

REGISTER_COMPONENT(hq_economy::component)
