#include <std_include.hpp>
#include "game/demonware/hq_logging.hpp"
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/demonware/achievement_engine.hpp"
#include "component/scheduler.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include <charconv>
#include "game/demonware/hq_marketplace.hpp"
#include "game/demonware/hq_contract_catalog.hpp"
#include "game/demonware/hq_contract_clock.hpp"
#include "game/demonware/hq_zombies_catalog.hpp"
#include "game/demonware/hq_zombies_contract_catalog.hpp"
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
				for (const auto& [name, entry] : data.achievements) console::info("  %s kind %d: %s %u/%u activated %llu\n",
					name.c_str(), entry.kind, entry.status.c_str(), entry.progress, entry.target, entry.activation);
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
			if (name != "kill" && name != "headshot" && name != "payroll" && name != "rankup")
			{
				console::info("Usage: aeevent <kill|headshot|payroll|rankup> (changes local economy)\n");
				return;
			}
			static std::int64_t last{};
			const auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
			last = std::max(last + 1, timestamp);
			demonware::reward_game_events::event event{name == "payroll" ? "18" : name == "rankup" ? "14" : "1", last, {}};
			if (name == "headshot") event.parameters.push_back({"6", 1});
			console::info("[HQ event] diagnostic event %.*s: %s; use aefetch user to refresh the native cache\n",
				static_cast<int>(name.size()), name.data(),
				demonware::achievement_engine::submit_event(event) ? "saved" : "failed");
		}

		void load_loot_catalog()
		{
			static std::atomic_bool warned{};
			try
			{
				const auto* stats = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, "mp/StatsTable.csv", false).stringTable;
				if (stats && stats->values && stats->columnCount > 24)
				{
					std::vector<std::uint32_t> zombies_pool;
					for (int row = 0; row < stats->rowCount; ++row)
					{
						std::uint32_t id{};
						if (std::string_view{cell(stats, row, 0)} == "zombieconsumable" &&
							std::string_view{cell(stats, row, 20)} != "1" && std::string_view{cell(stats, row, 24)} != "1" &&
							parse_number(cell(stats, row, 18), id)) zombies_pool.push_back(id);
					}
					demonware::achievement_engine::set_loot_catalog(std::move(zombies_pool), true);
				}
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
				std::map<std::uint32_t, std::string> types;
				for (const auto id : pool)
				{
					const auto rarity = utils::hook::invoke<int>(0x652330_g, id);
					if (rarity >= 0) rarities[id] = static_cast<unsigned>(rarity);
					// 0x652330 calls this same GUID-column reader for rarity (29).
					const auto* type = utils::hook::invoke<const char*>(0xD1BA0_g, id, 0);
					if (type) types[id] = type;
				}
				demonware::hq_marketplace::set_rarities(rarities);
				demonware::hq_marketplace::set_item_types(types);
				demonware::achievement_engine::set_loot_catalog(std::move(pool));
			}
			catch (const std::exception& error)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] load_loot_catalog: %s\n", error.what());
			}
			catch (...)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] load_loot_catalog: unknown exception\n");
			}
		}

		void tick_contracts()
		{
			static std::atomic_bool warned{};
			try
			{
				static demonware::hq_contract_clock timer;
				const auto* mode = game::Dvar_FindMalleableVar("g_gametype");
				const bool current = game::CL_IsLocalClientInGame(0) && !*game::virtualLobby_Loaded &&
					mode && mode->current.string && std::string_view{mode->current.string} != "hub";
				timer.tick(current, demonware::hq_contract_clock::clock::now(), game::environment::is_zombies() ? 11 : 4);
			}
			catch (const std::exception& error)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] tick_contracts: %s\n", error.what());
			}
			catch (...)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] tick_contracts: unknown exception\n");
			}
		}

		bool zombies_catalog_refresh_pending{}; // main pipeline only
		void refresh_zombies_catalog()
		{
			if (!zombies_catalog_refresh_pending || !game::virtual_lobby_loaded() || !game::AE_GetUserContext(0)) return;
			const auto* active = reinterpret_cast<const unsigned*>(game::AE_ScheduledAchievementTaskData.get() + 0xC0);
			if (*active) return;
			char transaction[32]{};
			game::AE_GenerateTransactionId(transaction);
			if (game::AE_FetchScheduledChallenges(0, transaction)) zombies_catalog_refresh_pending = false;
		}

		void load_catalog()
		{
			static std::atomic_bool warned{};
			try
			{
				if (game::environment::is_zombies())
				{
					// Use the shipped names so both native name->ID and ID->name lookups
					// work. Do not invent IDs or bypass the resolver's bounds checks.
					const auto* definitions = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, "dw/dwgamechallenges.csv", false).stringTable;
					std::vector<demonware::hq_economy::achievement> catalog;
					std::map<std::string, demonware::hq_event_predicate::rule> rules;
					if (definitions) for (const auto& entry : demonware::hq_zombies_catalog::entries)
					{
						for (int row = 0; row < definitions->rowCount; ++row)
						{
							std::uint32_t id{}, kind{};
							if (std::string_view{cell(definitions, row, 1)} != entry.name ||
								!parse_number(cell(definitions, row, 0), id) || id != static_cast<unsigned>(entry.id) ||
								!parse_number(cell(definitions, row, 2), kind) || kind != static_cast<unsigned>(entry.kind)) continue;
							catalog.push_back(demonware::hq_zombies_catalog::achievement(entry));
							rules.emplace(entry.name, demonware::hq_event_predicate::rule{34, entry.predicate});
							break;
						}
					}
					if (catalog.size() != std::size(demonware::hq_zombies_catalog::entries))
					{
						demonware::hq_logging::safe_warn_once(warned, "[ZM economy] waiting for native challenge identities (%zu/%zu found)\n",
							catalog.size(), std::size(demonware::hq_zombies_catalog::entries));
						return;
					}
					// Contract identities are independently validated so a missing contract
					// asset does not suppress the already working daily/weekly Orders.
					std::size_t contracts{};
					for (const auto& entry : demonware::hq_zombies_contract_catalog::entries)
					{
						for (int row = 0; row < definitions->rowCount; ++row)
						{
							std::uint32_t id{}, kind{}, event{};
							if (std::string_view{cell(definitions, row, 1)} != entry.name ||
								!parse_number(cell(definitions, row, 0), id) || id != entry.id ||
								!parse_number(cell(definitions, row, 2), kind) || kind != 11 ||
								!parse_number(cell(definitions, row, 3), event) || event != 34) continue;
							catalog.push_back(demonware::hq_zombies_contract_catalog::achievement(entry));
							rules.emplace(entry.name, demonware::hq_event_predicate::rule{event, cell(definitions, row, 4)});
							++contracts;
							break;
						}
					}
					static std::size_t prior_contracts{};
					if (contracts != prior_contracts)
					{
						zombies_catalog_refresh_pending = true;
						console::info("[ZM economy] %zu native kind-11 contracts ready\n", contracts);
						prior_contracts = contracts;
					}
					static bool announced{};
					if (!std::exchange(announced, true))
					{
						zombies_catalog_refresh_pending = true;
						console::info("[ZM economy] catalog ready: 6 daily / 3 weekly orders, native kinds 8/9\n");
					}
					demonware::achievement_engine::set_event_rules(std::move(rules));
					demonware::achievement_engine::set_catalog(std::move(catalog));
					return;
				}
				const auto* daily = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, "mp/dailychallengestable.csv", false).stringTable;
				const auto* definitions = game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, "dw/dwgamechallenges.csv", false).stringTable;
				if (!daily || !definitions) return;
				std::vector<demonware::hq_economy::achievement> catalog;
				// Daily order pool. The rotation shows 6 per day, cycling through the
				// full pool. All challenge names have event rules in dwgamechallenges.csv.
				for (const auto& [name, target, currency, amount] :
					std::vector<std::tuple<const char*, unsigned, unsigned, unsigned>>{
					{"daily_ch_kills", 25, 0, 1}, {"daily_ch_headshots", 3, 0, 1},
					{"daily_ch_assault_kills", 35, 0, 1}, {"daily_ch_smg_kills", 30, 0, 1},
					{"daily_ch_lmg_kills", 25, 0, 1}, {"daily_ch_shotgun_kills", 25, 0, 1},
					{"daily_ch_sniper_kills", 15, 0, 1}, {"daily_ch_pistol_kills", 10, 0, 1},
					{"daily_ch_shovel_kills", 5, 1, 500},
					{"daily_ch_assault_headshots", 5, 0, 1}, {"daily_ch_sniper_headshots", 3, 0, 1},
					{"daily_ch_lmg_headshots", 5, 0, 1},
					{"daily_ch_dom_wins", 1, 0, 1}, {"daily_ch_tdm_wins", 1, 0, 1},
					{"daily_ch_killstreak", 3, 1, 300}, {"daily_ch_dom_caps", 5, 1, 250},
					{"daily_ch_assists", 10, 1, 250}, {"daily_ch_destroy_scorestreaks", 2, 1, 300},
					{"daily_ch_equipment_kills", 5, 0, 1}, {"daily_ch_ffa_killer", 15, 0, 1}})
				{
					demonware::hq_economy::achievement entry;
					entry.name = entry.challenge_name = name; entry.target = target;
					entry.rewards = {{currency ? "GRANT_CURRENCY" : "GRANT_PRODUCT", currency ? currency : 1, amount}};
					catalog.push_back(entry);
				}
				// Weekly order pool. The rotation shows 3 per week.
				for (const auto& [name, target] : std::vector<std::pair<const char*, unsigned>>{
					{"weekly_ch_kills", 500}, {"weekly_ch_wins", 10}, {"weekly_ch_scorestreak_calls", 25},
					{"weekly_ch_infantry_kills", 100}, {"weekly_ch_airborne_kills", 100},
					{"weekly_ch_armored_kills", 100}, {"weekly_ch_mountain_kills", 100},
					{"weekly_ch_expeditionary_kills", 100},
					{"weekly_ch_long_range_kills", 50}, {"weekly_ch_explosive_kills", 50}})
				{
					demonware::hq_economy::achievement entry;
					entry.name = entry.challenge_name = name; entry.kind = 2; entry.target = target;
					entry.rewards = {{"GRANT_PRODUCT", 2, 1}};
					catalog.push_back(entry);
				}
				for (const auto& definition : demonware::hq_contract_catalog::entries)
				{
					const auto weapon = *definition.item_reference ? game::BG_GetItemGUIDFromReference(definition.item_reference) : 0;
					if (*definition.item_reference && !weapon) continue; // fail closed if the asset is unavailable
					catalog.push_back(demonware::hq_contract_catalog::achievement(definition, weapon));
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
			catch (const std::exception& error)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] load_catalog: %s\n", error.what());
			}
			catch (...)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] load_catalog: unknown exception\n");
			}
		}
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated()) return;
			command::add("hqeconomy", [](const command::params& params) { print_state(params.size() > 1 && std::string_view{params[1]} == "reload"); });
			command::add("hqgrant", grant);
			command::add("aeevent", event_command);
			scheduler::loop(load_loot_catalog, scheduler::pipeline::main, 5s);
			scheduler::loop(load_catalog, scheduler::pipeline::main, 5s);
			if (game::environment::is_zombies()) scheduler::loop(refresh_zombies_catalog, scheduler::pipeline::main, 1s);
			// Only the current mode's contracts accrue match time.
			scheduler::loop(tick_contracts, scheduler::pipeline::main, 1s);
			console::info("[HQ economy] native economy enabled for %s\n", game::environment::is_zombies() ? "Zombies" : "Multiplayer");
		}
	};
}

REGISTER_COMPONENT(hq_economy::component)
