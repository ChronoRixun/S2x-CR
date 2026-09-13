#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/types/demonware.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include <utils/hook.hpp>
#include "game/demonware/hq_vendor.hpp"
#include "game/demonware/hq_payroll.hpp"
#include "game/demonware/hq_products.hpp"
#include "game/demonware/hq_mail.hpp"
#include "game/demonware/hq_inventory_cache.hpp"
#include "component/scheduler.hpp"
#include "hq_vendor_globals.hpp"

namespace hq_native
{
	namespace
	{
		utils::hook::detour sku_success_hook;
		utils::hook::detour purchase_hook;
		utils::hook::detour sku_failure_hook;
		utils::hook::detour conversion_success_hook;
		utils::hook::detour conversion_failure_hook;
		std::atomic_uint32_t conversion_successes{}, conversion_failures{};
		// Distance between the queued 242 reply and its native callback, and the
		// game-thread stall record: build/research/quartermaster-fable.md explains why
		// both numbers decide whether a vendor walk can be trusted at all.
		std::atomic<std::int64_t> last_conversion_round_trip_ms{-1};
		std::atomic_bool last_conversion_succeeded{};
		constexpr std::int64_t stall_threshold_ms = 2000;
		std::atomic_uint32_t stall_count{};
		std::atomic<std::int64_t> longest_stall_ms{};
		std::atomic<std::uint64_t> longest_stall_lines{};

		std::int64_t conversion_round_trip()
		{
			const auto queued = demonware::hq_vendor::last_reply_ms.load();
			return queued ? demonware::hq_vendor::now_ms() - queued : -1;
		}

		// Main pipeline, every 100 ms: a gap far above that means Com_Frame did not run,
		// so neither did the Demonware pump that completes tasks (it lives on this thread).
		void watchdog_tick()
		{
			static std::int64_t last_tick{};
			static std::uint64_t last_lines{};
			const auto now = demonware::hq_vendor::now_ms();
			const auto lines = console::lines_printed();
			if (last_tick)
			{
				const auto gap = now - last_tick;
				if (gap > stall_threshold_ms)
				{
					++stall_count;
					if (gap > longest_stall_ms.load())
					{
						longest_stall_ms = gap;
						longest_stall_lines = lines - last_lines;
					}
					console::warn("[HQ watchdog] main loop gap %lld ms (%llu console lines printed meanwhile; level loads are expected, vendor/hub stalls are not)\n",
						gap, lines - last_lines);
				}
			}
			last_tick = now;
			last_lines = lines;
		}
		// 7F6FBB8 holds 13 fixed currency slots of 0x38 bytes each (wallet_status walks them).
		constexpr unsigned native_wallet_slots = 13;

		void wallet_status()
		{
			console::info("[HQ wallet] ready=%u count=%u ArmoryCredits=%u (Inventory_GetCurrencyBalance)\n",
				*reinterpret_cast<const unsigned char*>(0x7F6FE94_g),
				*reinterpret_cast<const unsigned*>(0x7F6FE90_g),
				utils::hook::invoke<unsigned>(0x279780_g, 0, unsigned(demonware::hq_economy::armory_credits)));
			for (unsigned i = 0; i < 13; ++i)
			{
				const auto* slot = reinterpret_cast<const unsigned char*>(0x7F6FBB8_g) + i * 0x38;
				console::info("[HQ wallet] slot %u currency=%u balance=%u\n", i, slot[0x20],
					*reinterpret_cast<const unsigned*>(slot + 0x24));
			}
		}

		void sync_wallet()
		{
			// Never race the initial native balance fetch or run native UI on the DW thread.
			if (!*reinterpret_cast<const unsigned char*>(0x7F6FE94_g)) return;
			try
			{
				std::optional<demonware::hq_payroll::push> notification;
				{
					std::lock_guard lock{demonware::hq_payroll::notification_mutex};
					notification.swap(demonware::hq_payroll::notification);
				}
				if (notification)
				{
					auto* bridge = game::AE_UserAchievementTaskData.get() + 0xF8;
					if (game::AE_SetResponseString(bridge, notification->json.c_str()))
					{
						demonware::hq_protocol::trace("payroll_native_push", notification->json);
						// 13C480 is the achievement push handler, distinct from task replies. It
						// resolves the record name to an achievement ID, updates the native user
						// achievement table and raises the LUI event the mail kiosk waits for:
						// achievementEngine {eventType 0 = CompletionUpdate, success, ID, kind}.
						utils::hook::invoke<void>(0x13C480_g, 0, bridge);
						console::info("[HQ payroll] delivered completion push: %s\n", notification->summary.c_str());
					}
					else
					{
						std::lock_guard lock{demonware::hq_payroll::notification_mutex};
						if (!demonware::hq_payroll::notification) demonware::hq_payroll::notification = std::move(notification);
					}
				}
				const auto data = demonware::hq_economy::snapshot();
				// The native wallet is a fixed 13-slot table; never hand it more distinct
				// currency ids than it can hold, whatever the store file contains.
				unsigned pushed{};
				for (const auto& [id, amount] : data.currencies)
				{
					if (!id || pushed >= native_wallet_slots) continue;
					++pushed;
					if (utils::hook::invoke<unsigned>(0x279780_g, 0, unsigned(id)) == amount) continue;
					// Native absolute setter + inventory eventType 5; no second grant.
					utils::hook::invoke<void>(0x27D510_g, 0, unsigned(id), amount);
				}
			}
			catch (const std::exception& error)
			{
				static bool warned{};
				if (!std::exchange(warned, true)) console::warn("[HQ wallet] sync failed: %s\n", error.what());
			}
			catch (...)
			{
				static bool unknown{};
				if (!std::exchange(unknown, true)) console::warn("[HQ wallet] sync failed with an unknown exception\n");
			}
		}

		void status()
		{
			console::info("[HQ native] SKUs fetched=%u (raw flag); use aecache for Orders\n",
				*reinterpret_cast<const unsigned char*>(0x81038A8_g));
		}


		// Dedicated storage: never enlarge a loop writing the original 400-entry
		// cache or the 400-element Lua binding stack buffer.
		struct alignas(8) native_sku_record { std::array<unsigned char, 0x2E8> bytes{}; };
		std::map<unsigned, native_sku_record> native_skus;
		utils::hook::detour sku_lookup_hook, sku_ids_hook, collection_price_hook;

		unsigned long long collection_price(const unsigned id)
		{
			const auto entry = demonware::hq_marketplace::find_sku(id);
			return entry ? (std::uint64_t{entry->price} << 32) | demonware::hq_economy::armory_credits : 0;
		}

		unsigned sku_lookup(const unsigned id, void** output)
		{
			const auto entry = demonware::hq_marketplace::find_sku(id);
			if (!entry) return sku_lookup_hook.invoke<unsigned>(id, output);
			auto [it, inserted] = native_skus.try_emplace(id);
			auto* bytes = it->second.bytes.data();
			if (inserted)
			{
				utils::hook::invoke<void>(0x20D440_g, bytes + 0x10);
				const auto put = [&](const std::size_t offset, const unsigned value) { std::memcpy(bytes + offset, &value, 4); };
				put(0, id); put(4, 100); put(8, id); put(12, 1);
				put(0x30, id); put(0x34, 1); // first initialized product record
				put(0x240, id); bytes[0x244] = 1; put(0x248, 1);
				put(0x24C, demonware::hq_economy::armory_credits); bytes[0x2E1] = 1;
				// +0x29C is the SKU data string Engine.Inventory_GetSKUInfoSKUData returns.
				// QuarterMasterUtils.FindSkuDataByType parses it as "key:value;key:value";
				// the decimal GUID that used to sit here parsed to nothing, which left the
				// Quartermaster's "MP"/"ZM" tag lookups empty and asserted in buildItems.
				const auto text = std::string_view{entry->data}.substr(0, 63);
				std::memcpy(bytes + 0x29C, text.data(), text.size()); bytes[0x29C + text.size()] = 0;
				// +0x25C is the promotional text Engine.Inventory_GetSKUInfo returns: the
				// binding 0x11FF90 emits promotionalText from record+0x25C and skuData from
				// record+0x29C, so this field is the same 64 bytes. ProcessSkuInfo splits it
				// on ';' into the tile's name and description, both passed to Engine.Localize.
				const auto promo = std::string_view{entry->promotional_text}.substr(0, 63);
				std::memcpy(bytes + 0x25C, promo.data(), promo.size()); bytes[0x25C + promo.size()] = 0;
			}
			std::memcpy(bytes + 0x250, &entry->price, 4);
			if (output) *output = bytes;
			return 0; // successful lookup; consumers of this hook use the output pointer
		}

		int sku_ids(game::hks::lua_State* state)
		{
			const auto valid = state->m_apistack.top - state->m_apistack.base == 1 &&
				state->m_apistack.base->t == game::hks::TNUMBER;
			const auto type = valid ? state->m_apistack.base->v.number : 0.0f;
			const auto entries = demonware::hq_marketplace::catalog();
			const auto ready = *reinterpret_cast<const unsigned char*>(0x81038A8_g) != 0;
			const auto count = ready && (type == 100.0f || type == 150.0f) ? entries.size() : 0;
			game::hks::HksObject table{}; table.t = game::hks::TTABLE;
			table.v.table = game::hks::Hashtable_Create(state, static_cast<unsigned>(count), 0);
			*state->m_apistack.top++ = table; // GC root throughout string allocation
			for (std::size_t i = 0; i < count; ++i)
			{
				game::hks::HksObject key{}; key.t = game::hks::TNUMBER; key.v.number = static_cast<float>(i + 1);
				// Same GUID-string conversion as native120760, preserving exact integers.
				utils::hook::invoke<void>(0xCAF40_g, state, entries[i].id);
				game::hks::hks_obj_settable(state, &table, &key, state->m_apistack.top - 1);
				--state->m_apistack.top;
			}
			return 1;
		}

		void refresh_item(const demonware::hq_economy::item& entry)
		{
			const auto item = demonware::hq_inventory_cache::project(entry, static_cast<std::uint64_t>(time(nullptr)));
			utils::hook::invoke<unsigned>(0x27DD30_g, 0, &item, 0, 0, entry.metadata.data(), static_cast<unsigned char>(entry.metadata.size()));
		}

		void sync_inventory()
		{
			// Wait for165's native callback; never seed or reset its ready flag.
			if (!*reinterpret_cast<const unsigned char*>(0x80385A8_g)) return;
			try
			{
				const auto data = demonware::hq_economy::snapshot();
				const auto now = static_cast<std::uint64_t>(time(nullptr));
				bool changed{};
				for (const auto& [key, entry] : data.inventory)
				{
					// HQ grants/drops/purchases use collision0. Do not collapse a
					// foreign collision record into this native GUID-only cache.
					if (entry.collision || !entry.guid || entry.metadata.size() > 64) continue;
					const auto expected = demonware::hq_inventory_cache::project(entry, now).quantity;
					const unsigned* native{};
					utils::hook::invoke<void>(0x279300_g, 0, entry.guid, &native);
					const auto quantity = native ? native[1] : 0;
					if (quantity == expected) continue;
					refresh_item(entry);
					changed = true;
					demonware::hq_protocol::trace("inventory_native_refresh", std::to_string(entry.guid) + ":" + std::to_string(quantity) + "->" + std::to_string(expected));
				}
				if (changed)
				{
					utils::hook::invoke<void>(0xD5F30_g, 0);
					utils::hook::invoke<void>(0x2752E0_g, 0, 2);
				}
			}
			catch (const std::exception& error)
			{
				static bool warned{};
				if (!std::exchange(warned, true)) console::warn("[HQ inventory] sync failed: %s\n", error.what());
			}
			catch (...)
			{
				static bool unknown{};
				if (!std::exchange(unknown, true)) console::warn("[HQ inventory] sync failed with an unknown exception\n");
			}
		}

		void purchase_entry(const unsigned controller, const unsigned id, const unsigned quantity, void* transaction, const int type)
		{
			if (!transaction) return;
			std::memset(transaction, 0, 25);
			if (controller != 0 || (type != 0 && type != 100 && type != 150)) return;
			// Keep native transaction generation and eventType24 completion contract.
			std::array<unsigned char, 25> tx{};
			utils::hook::invoke<void>(0x8390A0_g, tx.data());
			const auto* text = utils::hook::invoke<const char*>(0x839020_g, tx.data());
			if (!text) return;
			const std::string key{text, strnlen(text, 25)};
			if (key.empty() || key.size() >= 25) return;
			std::memcpy(transaction, tx.data(), tx.size());
			// Next main tick lets Lua register its transaction listener before completion.
			scheduler::once([tx, key, id, quantity]
			{
				unsigned error = game::demonware::BD_MARKETPLACE_STORAGE_ERROR;
				try
				{
					error = demonware::hq_marketplace::purchase(key, id, quantity);
					if (!error)
					{
						const auto data = demonware::hq_economy::snapshot();
						utils::hook::invoke<void>(0x27D510_g, 0, unsigned(demonware::hq_economy::armory_credits), data.currencies.at(demonware::hq_economy::armory_credits));
						refresh_item(data.inventory.at({id, 0}));
						utils::hook::invoke<void>(0xD5F30_g, 0);
						utils::hook::invoke<void>(0x2752E0_g, 0, 2);
					}
				}
				catch (const std::exception& e) { console::warn("[HQ purchase] %s\n", e.what()); }
				catch (...) { console::warn("[HQ purchase] unknown exception\n"); }
				demonware::hq_protocol::trace("native_purchase", key + ":" + std::to_string(id) + ":" + std::to_string(quantity) + ":error=" + std::to_string(error));
				console::info("[HQ purchase] sku=%u quantity=%u error=%u tx=%s\n", id, quantity, error, key.c_str());
				utils::hook::invoke<void>(0x275360_g, 0, 24, error == 0, tx.data());
			}, scheduler::pipeline::main);
		}

		// Slots of the native SKU cache (0x81038B0, 400 x 0x2E8) that already hold a
		// product record (id at +0x240): the task-99 pump stops asking once every slot
		// with a product id (+0x08) has one.
		std::pair<unsigned, unsigned> product_coverage()
		{
			unsigned expected{}, loaded{};
			for (unsigned i = 0; i < 400; ++i)
			{
				const auto* sku = reinterpret_cast<const unsigned char*>(0x81038B0_g) + i * 0x2E8;
				if (!*reinterpret_cast<const unsigned*>(sku) || !*reinterpret_cast<const unsigned*>(sku + 8)) continue;
				++expected;
				if (*reinterpret_cast<const unsigned*>(sku + 0x240) == *reinterpret_cast<const unsigned*>(sku + 8)) ++loaded;
			}
			return {expected, loaded};
		}

		// `full` prints every native SKU slot and the recovered globals (~500 lines).
		// The default keeps the output small: with a 400-entry catalog the old dump
		// alone was ~5 s of synchronous console output on the game thread.
		void vendor_status(const bool full)
		{
			status();
			wallet_status();
			unsigned sku_count{};
			constexpr unsigned sku_preview = 3;
			for (unsigned i = 0; i < 400; ++i)
			{
				const auto* sku = reinterpret_cast<const unsigned char*>(0x81038B0_g) + i * 0x2E8;
				if (!*reinterpret_cast<const unsigned*>(sku)) continue;
				++sku_count;
				if (!full && sku_count > sku_preview) continue;
				console::info("[HQ vendor] SKU slot=%u id=%u type=%u max=%u prices=%u product=%u items=%u\n", i,
					*reinterpret_cast<const unsigned*>(sku), *reinterpret_cast<const unsigned*>(sku + 4),
					*reinterpret_cast<const unsigned*>(sku + 12), sku[0x2E1],
					*reinterpret_cast<const unsigned*>(sku + 0x240), sku[0x244]);
			}
			if (!full && sku_count > sku_preview)
				console::info("[HQ vendor] ... %u more SKU slot(s); `hqvendor full` lists them and the raw globals\n", sku_count - sku_preview);
			const auto [products_expected, products_loaded] = product_coverage();
			console::info("[HQ vendor] catalogType=%u nonzeroSKUs=%u productsLoaded=%u/%u (task 99 queries=%u) inventoryAndBalanceReady=%u\n",
				*reinterpret_cast<const unsigned*>(0x81038AC_g), sku_count, products_loaded, products_expected,
				demonware::hq_products::requests.load(), utils::hook::invoke<bool>(0x27A210_g, 0));
			if (full) for (const auto offset : vendor_globals)
			{
				const auto address = 0x0_g + offset;
				MEMORY_BASIC_INFORMATION region{};
				if (!VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region)) ||
					region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
					address - reinterpret_cast<std::uintptr_t>(region.BaseAddress) + 8 > region.RegionSize) continue;
				std::uint64_t raw{};
				std::memcpy(&raw, reinterpret_cast<const void*>(address), sizeof(raw));
				console::info("[HQ vendor global] offset=%llX raw8=%016llX (table base or scalar; see lui-vendor-bindings.txt)\n",
					offset, raw);
			}
			console::info("[HQ vendor] Engine.Inventory_AreSKUsFetched=%u; 242 requests=%u replies=%u rejected=%u (conversion rule)\n",
				utils::hook::invoke<bool>(0x278400_g), demonware::hq_vendor::requests.load(),
				demonware::hq_vendor::replies.load(), demonware::hq_vendor::rejected.load());
			for (const auto* name : {"allow_hub_vendor_menu", "spv_hub_vendors_kswitch",
				"spv_hub_quartermasterVendor_kswitch", "spv_hub_payrollVendor_kswitch"})
			{
				auto* value = game::Dvar_FindMalleableVar(name);
				console::info("[HQ vendor] %s=%s\n", name, value ? game::Dvar_ValueToString(value, true, &value->current) : "<unregistered>");
			}
			console::info("[HQ vendor] inventoryReady=%u inventoryCount=%u dirtyMetadata=%u\n",
				*reinterpret_cast<const unsigned char*>(0x80385A8_g),
				*reinterpret_cast<const unsigned*>(0x80385A4_g), *reinterpret_cast<const unsigned*>(0x819B568_g));
			console::info("[HQ vendor] fullCollectionSKUs=%u; booster type0/common=%u type1/rare=%u (native quantity reader)\n",
				unsigned(demonware::hq_marketplace::catalog().size()),
				utils::hook::invoke<unsigned>(0x2AEEA0_g, 0, 0), utils::hook::invoke<unsigned>(0x2AEEA0_g, 0, 1));
			console::info("[HQ vendor] conversion successes=%u failures=%u responseTx=%.*s scalar=%llu currencyCount=%u inventoryCount=%u extraCount=%u\n",
				conversion_successes.load(), conversion_failures.load(), 24, reinterpret_cast<const char*>(0x81960C0_g),
				*reinterpret_cast<const std::uint64_t*>(0x81960E0_g),
				*reinterpret_cast<const unsigned*>(0x8196254_g), *reinterpret_cast<const unsigned*>(0x8196264_g),
				*reinterpret_cast<const unsigned*>(0x8196274_g));
			console::info("[HQ vendor] conversion last reply queued %lld ms ago; last callback %s after %lld ms (healthy: a few ms; ~30000 = timed out behind a game-thread stall)\n",
				conversion_round_trip(), last_conversion_round_trip_ms.load() < 0 ? "none" : last_conversion_succeeded.load() ? "success" : "FAILURE",
				last_conversion_round_trip_ms.load());
			console::info("[HQ watchdog] main loop gaps >%lld ms: %u, longest %lld ms with %llu console lines in it\n",
				stall_threshold_ms, stall_count.load(), longest_stall_ms.load(), longest_stall_lines.load());
			console::info("[HQ vendor] entitlement fetched flag and final LUI enable expression unresolved\n");
		}

		void vendor_status_command(const command::params& params)
		{
			vendor_status(params.size() > 1 && std::string_view{params[1]} == "full");
		}

		// The callbacks run on the game thread: one line each, never the full dump,
		// otherwise the diagnostic itself stalls the frame it is diagnosing.
		void conversion_success(void* task)
		{
			conversion_success_hook.invoke<void>(task);
			++conversion_successes;
			const auto round_trip = conversion_round_trip();
			last_conversion_round_trip_ms = round_trip;
			last_conversion_succeeded = true;
			console::info("[HQ vendor] conversion-rule native success callback %lld ms after the 242 reply was queued (successes=%u failures=%u); `hqvendor` for state\n",
				round_trip, conversion_successes.load(), conversion_failures.load());
		}

		void conversion_failure(void* task)
		{
			conversion_failure_hook.invoke<void>(task);
			++conversion_failures;
			const auto round_trip = conversion_round_trip();
			last_conversion_round_trip_ms = round_trip;
			last_conversion_succeeded = false;
			console::warn("[HQ vendor] conversion-rule native FAILURE callback %lld ms after the 242 reply was queued (successes=%u failures=%u): %s\n",
				round_trip, conversion_successes.load(), conversion_failures.load(),
				round_trip >= 20000 ? "the reply sat unread for the whole task timeout, i.e. the game thread was stalled (see [HQ watchdog])" :
				round_trip < 0 ? "no 242 reply was ever queued" : "the client rejected a reply it received promptly; compare the 242 dumps");
		}

		void mail_status()
		{
			console::info("[HQ mail native] policy=empty inbox; reads=%u redeemsSuppressed=%u invalidIndices=%u count=%u capacity=%u\n",
				demonware::hq_mail::native_reads.load(), demonware::hq_mail::native_redeems.load(),
				demonware::hq_mail::rejected_indices.load(), *reinterpret_cast<const unsigned*>(0x8A1501C_g),
				*reinterpret_cast<const unsigned*>(0x8A15018_g));
			const auto ready = *reinterpret_cast<const int*>(0x8A14F84_g);
			const auto* slots = *reinterpret_cast<const unsigned char* const*>(0x8A15010_g);
			console::info("[HQ mail native] ready=%d slots=%p; first 14 advertised slots\n", ready, slots);
			if (!ready || !slots || *reinterpret_cast<const unsigned*>(0x8A1501C_g) < 14) return;
			MEMORY_BASIC_INFORMATION memory{};
			if (!VirtualQuery(slots, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
				(memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ||
				reinterpret_cast<std::uintptr_t>(slots) - reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + 14 * 0x1CA0 > memory.RegionSize)
			{
				console::warn("[HQ mail native] slot region unavailable\n");
				return;
			}
			for (unsigned i = 0; i < 14; ++i)
			{
				const auto* slot = slots + i * 0x1CA0;
				console::info("[HQ mail native] slot %u id=%llu contentLength=%u codeLength=%u\n", i,
					*reinterpret_cast<const std::uint64_t*>(slot + 0x10),
					*reinterpret_cast<const unsigned*>(slot + 0x102C),
					*reinterpret_cast<const unsigned*>(slot + 0x1C34));
			}
		}

		void task99(const command::params&)
		{
			std::string payload;
			{
				std::lock_guard lock{demonware::hq_products::signature_mutex};
				payload = demonware::hq_products::signature;
			}
			const auto [expected, loaded] = product_coverage();
			console::info("[HQ task99] product queries=%u rejected=%u products served=%u; native SKU slots with product %u/%u; last request [%s]\n",
				demonware::hq_products::requests.load(), demonware::hq_products::rejected.load(),
				demonware::hq_products::products.load(), loaded, expected, payload.c_str());
		}

		void open_drop(const command::params& params)
		{
			const std::string_view drop = params.size() == 2 ? params[1] : "";
			if (drop != "common" && drop != "rare")
			{
				console::info("Usage: hqopendrop <common|rare> (consumes one owned drop)\n");
				return;
			}
			char transaction[32]{};
			game::AE_GenerateTransactionId(transaction);
			const auto issued = utils::hook::invoke<bool>(0x2B0850_g, 0, drop == "common" ? 0u : 1u, transaction);
			console::info("[HQ native] open drop %s, Tx=%s\n", issued ? "issued" : "rejected", transaction);
		}

		void sku_success(void* task)
		{
			sku_success_hook.invoke<void>(task);
			console::info("[HQ native] SKU page success callback\n");
			status();
		}

		void sku_failure(void* task)
		{
			sku_failure_hook.invoke<void>(task);
			console::warn("[HQ native] SKU page failure callback\n");
			status();
		}
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated() || game::environment::is_zombies()) return;
			sku_success_hook.create(0x27B700_g, sku_success);
			purchase_hook.create(0x276580_g, purchase_entry);
			sku_ids_hook.create(0x120760_g, sku_ids);
			sku_lookup_hook.create(0x2797E0_g, sku_lookup);
			collection_price_hook.create(0x274970_g, collection_price);
			sku_failure_hook.create(0x27B6C0_g, sku_failure);
			conversion_success_hook.create(0x27A4C0_g, conversion_success);
			conversion_failure_hook.create(0x27A460_g, conversion_failure);
			command::add("hqnative", status);
			command::add("hqwallet", wallet_status);
			scheduler::loop(sync_wallet, scheduler::pipeline::main, 100ms);
			scheduler::loop(sync_inventory, scheduler::pipeline::main, 100ms);
			command::add("hqvendor", vendor_status_command);
			scheduler::loop(watchdog_tick, scheduler::pipeline::main, 100ms);
			command::add("hqmail", mail_status);
			command::add("hqopendrop", open_drop);
			command::add("hqtask99", task99);
		}
	};
}

REGISTER_COMPONENT(hq_native::component)
