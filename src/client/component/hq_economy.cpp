#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/demonware/achievement_engine.hpp"
#include "component/scheduler.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include <charconv>

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
					// Column 10 is XP; currency 2/25 is a documented local AC reward,
					// pending a verified XP reward mapping.
					entry.rewards = {{"GRANT_CURRENCY", 2, 25}};
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
					entry.rewards = {{"GRANT_CURRENCY", 2, 100}};
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
				catalog.push_back(entry);
			}
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
			scheduler::loop(load_catalog, scheduler::pipeline::main, 5s);
		}
	};
}

REGISTER_COMPONENT(hq_economy::component)
