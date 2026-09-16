#include <std_include.hpp>
#include "game/demonware/hq_logging.hpp"
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/types/demonware.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include <utils/hook.hpp>
#include "game/demonware/hq_vendor.hpp"
#include "game/demonware/hq_payroll.hpp"
#include "game/demonware/achievement_engine.hpp"
#include "game/demonware/hq_products.hpp"
#include "game/demonware/hq_mail.hpp"
#include "game/demonware/hq_inventory_cache.hpp"
#include "component/scheduler.hpp"
#include "hq_vendor_globals.hpp"
#include "game/ui_scripting/execution.hpp"
#include "ui_scripting.hpp"
#include <charconv>
#include <set>

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
			static std::atomic_bool warned{};
			try
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
						demonware::hq_logging::safe_warn("[HQ watchdog] main loop gap %lld ms (%llu console lines printed meanwhile; level loads are expected, vendor/hub stalls are not)\n",
							gap, lines - last_lines);
					}
				}
				last_tick = now;
				last_lines = lines;
			}
			catch (const std::exception& error)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] watchdog_tick: %s\n", error.what());
			}
			catch (...)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] watchdog_tick: unknown exception\n");
			}
		}
		// 7F6FBB8 holds 13 fixed currency slots of 0x38 bytes each (wallet_status walks them).
		constexpr unsigned native_wallet_slots = demonware::hq_economy::native_wallet_slots;
		// Main-pipeline ownership, scoped to the native caches' ready lifetime.
		std::set<unsigned> managed_currencies, managed_items;
		bool inventory_notification_dirty{};

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
			static std::atomic_bool warned{};
			try
			{
				// Consume before readiness checks: unavailable caches drop this refresh, never retry forever.
				const auto refresh_achievements = demonware::achievement_engine::consume_event_cache_refresh();
				// Never race the initial native balance fetch or run native UI on the DW thread.
				if (!*reinterpret_cast<const unsigned char*>(0x7F6FE94_g)) { managed_currencies.clear(); return; }
				if (refresh_achievements && game::AE_GetUserContext(0))
				{
					// Borrow the payroll poll's main-thread boundary; the native fetch replaces
					// progress and status together, including the transition to claimable.
					char transaction[32]{};
					game::AE_GenerateTransactionId(transaction);
					game::AE_FetchUserAchievements(0, transaction);
				}
				std::optional<demonware::hq_payroll::push> notification;
				{
					std::lock_guard lock{demonware::hq_payroll::notification_mutex};
					notification.swap(demonware::hq_payroll::notification);
				}
				if (notification)
				{
					// 13C480 does NOT take a controller index: its first argument is the
					// Achievement Engine user context pointer (the retail caller 0x8327B0 passes
					// *(void**)(task + 0x30)), which it maps back to a controller index with
					// 0x7897A0 - a linear search of the context table that returns -1 for a null
					// pointer. A -1 controller makes the LUI instance lookup 0x4A0D90 fail, so the
					// achievementEngine event the mail kiosk waits for is never raised (hence the
					// "Unable to get payroll at this time" banner after a delivered push) and the
					// user achievement table is indexed at base - 0x13890. Never pass 0.
					auto* context = game::AE_GetUserContext(0);
					auto* bridge = game::AE_UserAchievementTaskData.get() + 0xF8;
					if (context && game::AE_SetResponseString(bridge, notification->json.c_str()))
					{
						demonware::hq_protocol::trace("payroll_native_push", notification->json);
						// 13C480 is the achievement push handler, distinct from task replies. It
						// resolves the record name to an achievement ID, updates the native user
						// achievement table and raises the LUI event the mail kiosk waits for:
						// achievementEngine {eventType 0 = CompletionUpdate, success, ID, kind}.
						utils::hook::invoke<void>(0x13C480_g, context, bridge);
						console::info("[HQ payroll] delivered completion push: %s (context %p resolves to controller %d)\n",
							notification->summary.c_str(), context, utils::hook::invoke<int>(0x7897A0_g, context));
					}
					else
					{
						// Requeue and make the stall visible. A null user context is the condition
						// that produced the "Unable to get payroll at this time" banner: without it
						// 13C480 would resolve controller -1, so the push is held back instead. That
						// is normally a tick or two around sign-in; if it never clears, the payroll
						// completion never reaches the kiosk, so warn (rate limited to one line per
						// five seconds, plus the first requeue) rather than failing silently.
						static std::chrono::steady_clock::time_point last_requeue_warning{};
						static unsigned long long requeues{};
						const auto now = std::chrono::steady_clock::now();
						if (!requeues++ || now - last_requeue_warning >= std::chrono::seconds{5})
						{
							last_requeue_warning = now;
							demonware::hq_logging::safe_warn("[HQ payroll] completion push requeued: %s (retry %llu); the kiosk"
								" will show \"Unable to get payroll at this time\" until it is delivered\n",
								context ? "AE_SetResponseString failed" : "AE_GetUserContext(0) is null", requeues);
						}
						std::lock_guard lock{demonware::hq_payroll::notification_mutex};
						if (!demonware::hq_payroll::notification) demonware::hq_payroll::notification = std::move(notification);
					}
				}
				const auto data = demonware::hq_economy::snapshot();
				// The native wallet is a fixed 13-slot table; never hand it more distinct
				// currency ids than it can hold, whatever the store file contains.
				// Only a successful snapshot can prove a previously managed key was removed.
				for (auto it = managed_currencies.begin(); it != managed_currencies.end();)
				{
					if (data.currencies.contains(static_cast<std::uint8_t>(*it))) { ++it; continue; }
					if (utils::hook::invoke<unsigned>(0x279780_g, 0, *it))
						utils::hook::invoke<void>(0x27D510_g, 0, *it, 0u); // setter emits wallet event
					it = managed_currencies.erase(it);
				}
				unsigned pushed{};
				for (const auto& [id, amount] : data.currencies)
				{
					if (!id || pushed >= native_wallet_slots) continue;
					++pushed;
					// Zeroing a balance does not free its slot. Account for all occupied
					// native IDs, including entries outside the current store projection.
					bool present{}, empty{};
					for (unsigned slot = 0; slot < native_wallet_slots; ++slot)
					{
						const auto currency = *(reinterpret_cast<const unsigned char*>(0x7F6FBB8_g) + slot * 0x38 + 0x20);
						present |= currency == id;
						empty |= currency == 0;
					}
					if (!present && !empty) continue;
					managed_currencies.insert(id);
					if (utils::hook::invoke<unsigned>(0x279780_g, 0, unsigned(id)) == amount) continue;
					// Native absolute setter + inventory eventType 5; no second grant.
					utils::hook::invoke<void>(0x27D510_g, 0, unsigned(id), amount);
				}
			}
			catch (const std::exception& error)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] sync_wallet: %s\n", error.what());
			}
			catch (...)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] sync_wallet: unknown exception\n");
			}
		}

		// `quiet` is for the callers that fire on every SKU page of every launch rather
		// than on a real event: `hqvendor` and the failure callback still report the flag
		// to the console, the success callback only does so in a debug build.
		void status(const bool quiet = false)
		{
			const auto fetched = *reinterpret_cast<const unsigned char*>(0x81038A8_g);
			if (quiet) console::debug("[HQ native] SKUs fetched=%u (raw flag); use aecache for Orders\n", fetched);
			else console::info("[HQ native] SKUs fetched=%u (raw flag); use aecache for Orders\n", fetched);
		}


		// Dedicated storage: never enlarge a loop writing the original 400-entry
		// cache or the 400-element Lua binding stack buffer.
		struct alignas(8) native_sku_record { std::array<unsigned char, 0x2E8> bytes{}; };
		// The record carries a fixed array of ten 0x38-byte item structs at +0x00: the
		// Inventory_GetSKUInfo binding 0x11FF90 reads item i's id at record + i * 0x38 +
		// 0x30 and its quantity at +0x34, looping over numItems (the byte at +0x244), so
		// the last usable slot ends at +0x230 where the container's own fields begin.
		constexpr std::size_t native_sku_items = 10;
		std::map<unsigned, native_sku_record> native_skus;
		utils::hook::detour sku_lookup_hook, sku_ids_hook, collection_price_hook;

		unsigned long long collection_price(const unsigned id)
		{
			const auto entry = demonware::hq_marketplace::find_sku(id);
			return entry ? (std::uint64_t{entry->price} << 32) | entry->currency : 0;
		}

		unsigned sku_lookup(const unsigned id, void** output)
		{
			const auto entry = demonware::hq_marketplace::find_sku(id);
			if (!entry) return sku_lookup_hook.invoke<unsigned>(id, output);
			auto [it, inserted] = native_skus.try_emplace(id);
			auto* bytes = it->second.bytes.data();
			if (inserted)
			{
				// 0x20D440 is not a per-item constructor: decompiled
				// (build/research/decomp-qm/20D440.c) it is the record's item-array
				// initialiser, zeroing +0x230/+0x234 and running the element initialiser
				// 0xA48770 - itself only "*(std::uint64_t*)(p + 0x20) = 0" - over all ten
				// elements at p + i * 0x38. It therefore writes nothing but zeros across
				// p + 0x00 .. p + 0x237, which a value-initialised native_sku_record already
				// is. Slice 7 called it once per granted item at bytes + 0x10 + i * 0x38;
				// for the five-item CWL packs i = 3 and i = 4 reached bytes + 0x2EF and
				// bytes + 0x327, past the 0x2E8 record and out of its std::map node, which
				// cleared a neighbouring node's _Left/_Parent/_Right and made the next
				// try_emplace fault on a null _Parent->_Color (+0x18). Do not call it.
				const auto put = [&](const std::size_t offset, const unsigned value) { std::memcpy(bytes + offset, &value, 4); };
				put(0, id); put(4, entry->type); put(8, id); put(12, 1);
				auto items = demonware::hq_marketplace::granted_items(*entry);
				// Multi-item grants stay native, but never past the tenth slot.
				if (items.size() > native_sku_items) items.resize(native_sku_items);
				for (std::size_t i = 0; i < items.size(); ++i)
				{
					put(0x30 + i * 0x38, items[i]); put(0x34 + i * 0x38, 1);
				}
				put(0x240, id); bytes[0x244] = static_cast<unsigned char>(items.size()); put(0x248, 1);
				put(0x24C, entry->currency); bytes[0x2E1] = 1;
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
			auto entries = demonware::hq_marketplace::catalog();
			if (type == 201.0f) std::erase_if(entries, [](const auto& entry) { return !*entry.contract; });
			const auto ready = *reinterpret_cast<const unsigned char*>(0x81038A8_g) != 0;
			const auto count = ready && (type == 100.0f || type == 150.0f || type == 201.0f) ? entries.size() : 0;
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
			if (!entry.guid || entry.collision || entry.metadata.size() > 64) return;
			managed_items.insert(entry.guid);
			const auto item = demonware::hq_inventory_cache::project(entry, static_cast<std::uint64_t>(time(nullptr)));
			inventory_notification_dirty = true; // Retain notification work even if a native call partially fails.
			utils::hook::invoke<unsigned>(0x27DD30_g, 0, &item, 0, 0, entry.metadata.data(), static_cast<unsigned char>(entry.metadata.size()));
			if (entry.metadata.empty())
			{
				// 27DD30 skips zero-length metadata; explicitly clear its 64 bytes and length.
				unsigned char* native{};
				utils::hook::invoke<void>(0x279300_g, 0, entry.guid, &native);
				if (native) std::memset(native + 0x20, 0, 65);
			}
		}

		void sync_inventory()
		{
			static std::atomic_bool warned{};
			try
			{
				// Wait for165's native callback; never seed or reset its ready flag.
				if (!*reinterpret_cast<const unsigned char*>(0x80385A8_g)) { managed_items.clear(); inventory_notification_dirty = false; return; }
				const auto data = demonware::hq_economy::snapshot();
				const auto now = static_cast<std::uint64_t>(time(nullptr));
				for (auto it = managed_items.begin(); it != managed_items.end();)
				{
					if (data.inventory.contains({*it, 0})) { ++it; continue; }
					refresh_item({*it, 0});
					it = managed_items.erase(it);
				}
				for (const auto& [key, entry] : data.inventory)
				{
					// HQ grants/drops/purchases use collision0. Do not collapse a
					// foreign collision record into this native GUID-only cache.
					if (entry.collision || !entry.guid || entry.metadata.size() > 64) continue;
					managed_items.insert(entry.guid);
					const auto projected = demonware::hq_inventory_cache::project(entry, now);
					const auto expected = projected.quantity;
					const unsigned* native{};
					utils::hook::invoke<void>(0x279300_g, 0, entry.guid, &native);
					const auto quantity = native ? native[1] : 0;
					// Repair legacy expiry sentinels too; quantity alone hid unusable items.
					const auto* cached = reinterpret_cast<const demonware::hq_inventory_cache::record*>(native);
					// 279300 returns a 0x68-byte record: metadata at +0x20, length at +0x60.
					// Compare at most 64 bytes directly: cheap here and cannot hide hash collisions.
					const auto* bytes = reinterpret_cast<const unsigned char*>(native);
					const auto metadata_matches = bytes ? bytes[0x60] == entry.metadata.size() &&
						std::memcmp(bytes + 0x20, entry.metadata.data(), entry.metadata.size()) == 0 : entry.metadata.empty();
					if (quantity == expected && metadata_matches && (!cached ||
						(cached->expires == projected.expires && cached->duration == projected.duration))) continue;
					refresh_item(entry);
					try
					{
						demonware::hq_protocol::trace("inventory_native_refresh", std::to_string(entry.guid) + ":" + std::to_string(quantity) + "->" + std::to_string(expected));
					}
					catch (...) {} // Optional diagnostics must not interrupt cache synchronization.
				}
				if (inventory_notification_dirty)
				{
					utils::hook::invoke<void>(0xD5F30_g, 0);
					utils::hook::invoke<void>(0x2752E0_g, 0, 2);
					inventory_notification_dirty = false;
				}
			}
			catch (const std::exception& error)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] sync_inventory: %s\n", error.what());
			}
			catch (...)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] sync_inventory: unknown exception\n");
			}
		}

		void purchase_entry(const unsigned controller, const unsigned id, const unsigned quantity, void* transaction, const int type)
		{
			if (!transaction) return;
			std::memset(transaction, 0, 25);
			if (controller != 0 || (type != 0 && type != 100 && type != 150 && type != 201)) return;
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
				static std::atomic_bool warned{};
				try
				{
					unsigned error = game::demonware::BD_MARKETPLACE_STORAGE_ERROR;
					try
					{
						error = demonware::hq_marketplace::purchase(key, id, quantity);
					}
					catch (const std::exception& e) { demonware::hq_logging::safe_warn_once(warned, "[HQ purchase] %s\n", e.what()); }
					catch (...) { demonware::hq_logging::safe_warn_once(warned, "[HQ purchase] unknown exception\n"); }
					// Persistence defines purchase success. Ready-checked sync catches refresh
					// failures and the existing polling loops retry without another debit.
					if (!error) { sync_wallet(); sync_inventory(); }
					try
					{
						demonware::hq_protocol::trace("native_purchase", key + ":" + std::to_string(id) + ":" + std::to_string(quantity) + ":error=" + std::to_string(error));
					}
					catch (...) {} // A settled purchase must still deliver its completion.
					demonware::hq_logging::safe_info("[HQ purchase] sku=%u quantity=%u error=%u\n", id, quantity, error);
					utils::hook::invoke<void>(0x275360_g, 0, 24, error == 0, tx.data());
				}
				catch (const std::exception& error)
				{
					demonware::hq_logging::safe_warn_once(warned, "[HQ callback] deferred purchase: %s\n", error.what());
				}
				catch (...)
				{
					demonware::hq_logging::safe_warn_once(warned, "[HQ callback] deferred purchase: unknown exception\n");
				}
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
			console::info("[HQ vendor] conversion successes=%u failures=%u scalar=%llu currencyCount=%u inventoryCount=%u extraCount=%u\n",
				conversion_successes.load(), conversion_failures.load(),
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
			console::info("[HQ mail native] policy=local deliveries; reads=%u redeems=%u invalidIndices=%u count=%u capacity=%u\n",
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
			console::info("[HQ native] open drop %s\n", issued ? "issued" : "rejected");
		}

		void sku_success(void* task)
		{
			sku_success_hook.invoke<void>(task);
			// Once per SKU page on every launch, and it says only that nothing went wrong;
			// the failure callback below still warns, and `hqvendor` reports the counts.
			console::debug("[HQ native] SKU page success callback\n");
			status(true);
		}

		// Drives the native SKU record population that the Quartermaster reaches through
		// Inventory_GetSKUInfo, without needing the vendor menu (which cannot be opened
		// from the console). The catalog leads with the two tagged supply drops, the
		// three contract SKUs and the five-item CWL packs, i.e. every shape the slice-7
		// out-of-bounds item writes crashed on.
		// `hqskutest` walks the head of the catalog; `hqskutest <decimal|0xGUID>` reports one
		// SKU, so the price currency a tile renders can be read back from the console. The
		// price fields are the ones Inventory_GetSKUInfo (0x11FF90) publishes to Lua as
		// prices[1]: currency at +0x24C, value at +0x250, count at +0x2E1.
		void sku_test(const command::params& params)
		{
			constexpr std::size_t sku_test_entries = 12;
			const auto entries = demonware::hq_marketplace::catalog();
			std::vector<std::uint32_t> requested;
			if (params.size() == 2)
			{
				std::string_view value = params[1];
				const auto hex = value.starts_with("0x") || value.starts_with("0X");
				if (hex) value.remove_prefix(2);
				unsigned guid{};
				const auto parsed = std::from_chars(value.data(), value.data() + value.size(), guid, hex ? 16 : 10);
				if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !guid ||
					!demonware::hq_marketplace::find_sku(guid))
				{
					console::info("Usage: hqskutest [decimal|0xGUID] (no id walks the first %u catalog SKUs)\n",
						static_cast<unsigned>(sku_test_entries));
					return;
				}
				requested.push_back(guid);
			}
			else
			{
				const auto count = entries.size() < sku_test_entries ? entries.size() : sku_test_entries;
				for (std::size_t i = 0; i < count; ++i) requested.push_back(entries[i].id);
			}
			for (const auto id : requested)
			{
				void* record{};
				const auto result = sku_lookup(id, &record);
				const auto* bytes = static_cast<const unsigned char*>(record);
				if (result != 0 || !bytes)
				{
					console::warn("[HQ skutest] id=%u lookup failed (result=%u)\n", id, result);
					continue;
				}
				std::string items;
				for (unsigned item = 0; item < bytes[0x244]; ++item)
				{
					if (!items.empty()) items += ",";
					items += std::to_string(*reinterpret_cast<const unsigned*>(bytes + 0x30 + item * 0x38));
				}
				console::info("[HQ skutest] id=%u type=%u price=%u currency=%u prices=%u numItems=%u items=[%s] product=%u data=\"%s\" promo=\"%s\"\n",
					id, *reinterpret_cast<const unsigned*>(bytes + 4),
					*reinterpret_cast<const unsigned*>(bytes + 0x250),
					*reinterpret_cast<const unsigned*>(bytes + 0x24C), bytes[0x2E1],
					bytes[0x244], items.data(), *reinterpret_cast<const unsigned*>(bytes + 0x240),
					reinterpret_cast<const char*>(bytes + 0x29C), reinterpret_cast<const char*>(bytes + 0x25C));
			}
			console::info("[HQ skutest] populated %u of %u catalog SKU record(s), no fault\n",
				static_cast<unsigned>(requested.size()), static_cast<unsigned>(entries.size()));
		}

		// The native user achievement table: 0x13890 bytes per controller, 1000 records of
		// 0x40 bytes, "user achievements fetched" byte at table - 0x10. The layout is the one
		// the record parser 0x13A570 writes and AE_GetPlayerAchievementInfo (0x1213B0) and
		// AE_GetPlayerActiveChallenges (0x121F40) read back (decompiles under
		// build/research/ghidra/decomp-payroll).
		constexpr std::size_t user_achievement_stride = 0x13890;
		constexpr std::size_t user_achievement_records = 1000;
		constexpr std::size_t user_achievement_size = 0x40;

		const unsigned char* user_achievement(const unsigned controller, const int id)
		{
			const auto* table = reinterpret_cast<const unsigned char*>(0x60A4090_g) + controller * user_achievement_stride;
			for (std::size_t i = 0; i < user_achievement_records; ++i)
			{
				const auto* record = table + i * user_achievement_size;
				if (*reinterpret_cast<const std::int32_t*>(record + 0xC) == id) return record;
			}
			return nullptr;
		}

		// Exactly the inputs the Headquarters Post kiosk decides on
		// (ui/s2/mail_officer_menu_uc.lua): "if 14400 <= timeSinceLastCompletion or
		// fullfilledTimes <= 0 then canCollectPayroll = true else secondsUntilNextPayroll =
		// 14400 - timeSinceLastCompletion end", where AE_GetPlayerAchievementInfo derives
		// timeSinceLastCompletion as now - record+0x30 and fullfilledTimes is record+0x2C.
		void payroll_state()
		{
			const auto now = static_cast<std::uint64_t>(time(nullptr));
			console::info("[HQ payroll] user achievements fetched=%d\n",
				static_cast<int>(*reinterpret_cast<const unsigned char*>(0x60A4080_g)));
			for (const auto id : {345, 757})
			{
				const auto* record = user_achievement(0, id);
				if (!record)
				{
					console::info("[HQ payroll] achievement %d: no native record\n", id);
					continue;
				}
				const auto fulfilled = *reinterpret_cast<const std::int32_t*>(record + 0x2C);
				const auto completion = *reinterpret_cast<const std::uint64_t*>(record + 0x30);
				const auto since = completion && completion <= now ? now - completion : 0;
				const auto collectable = since >= 14400 || fulfilled <= 0;
				const auto countdown = collectable ? std::string{"PAYROLL available"} : "countdown " + std::to_string(14400 - since) + "s";
				console::info("[HQ payroll] achievement %d kind %d status %d progress %u/%d fullfilledTimes %d "
					"lastCompletionTime %llu timeSinceLastCompletion %llu -> %s\n",
					id, *reinterpret_cast<const std::int32_t*>(record + 8),
					*reinterpret_cast<const std::int32_t*>(record + 0x38),
					*reinterpret_cast<const std::uint16_t*>(record + 0x28),
					*reinterpret_cast<const std::int32_t*>(record + 0x10),
					fulfilled, completion, since, countdown.c_str());
			}
		}
		// What the Quartermaster's contract path actually sees.
		// QuarterMasterUtils.GetContractCurrencies walks
		// Engine.Inventory_GetAllSKUIDs(SKUType.Quartermaster), reads key "c" out of
		// Engine.Inventory_GetSKUInfoSKUData and only keeps a contract when
		// Inventory_GetItemQuantity(controller, items[1].guid) > 0; the vendor's "VIEW
		// CONTRACT" option uses key "C" with AchievementEngineUtils.IsOrderAvailable, which
		// requires the same id in the scheduled challenge cache (`aecache` prints that).
		void contract_state()
		{
			static std::atomic_bool warned{};
			try
			{
				for (const auto& entry : demonware::hq_marketplace::catalog())
				{
					if (!*entry.contract) continue;
					const auto cached = native_skus.find(entry.id);
					if (cached == native_skus.end())
					{
						console::info("[HQ contracts] sku %u (%s) not cached\n", entry.id, entry.contract);
						continue;
					}
					const auto* bytes = cached->second.bytes.data();
					std::string items;
					for (unsigned item = 0; item < bytes[0x244]; ++item)
					{
						const auto guid = *reinterpret_cast<const unsigned*>(bytes + 0x30 + item * 0x38);
						const unsigned* native{};
						utils::hook::invoke<void>(0x279300_g, 0, guid, &native);
						if (!items.empty()) items += ",";
						items += std::to_string(guid) + "x" + std::to_string(native ? native[1] : 0);
					}
					console::info("[HQ contracts] sku %u (%s) type=%u price=%u currency=%u data=\"%s\" owned=[%s]\n",
						entry.id, entry.contract, *reinterpret_cast<const unsigned*>(bytes + 4),
						*reinterpret_cast<const unsigned*>(bytes + 0x250),
						*reinterpret_cast<const unsigned*>(bytes + 0x24C),
						reinterpret_cast<const char*>(bytes + 0x29C), items.data());
				}
				const auto* table = reinterpret_cast<const unsigned char*>(0x60A4090_g);
				for (std::size_t i = 0; i < user_achievement_records; ++i)
				{
					const auto* record = table + i * user_achievement_size;
					const auto id = *reinterpret_cast<const std::int32_t*>(record + 0xC);
					if (id == -1 || *reinterpret_cast<const std::int32_t*>(record + 8) != 4) continue;
					console::info("[HQ contracts] native kind 4 record id %d status %d progress %u/%d timeLimit %d "
						"timeLeft %d expires %llu reward %p\n", id,
						*reinterpret_cast<const std::int32_t*>(record + 0x38),
						*reinterpret_cast<const std::uint16_t*>(record + 0x28),
						*reinterpret_cast<const std::int32_t*>(record + 0x10),
						*reinterpret_cast<const std::int32_t*>(record + 4),
						*reinterpret_cast<const std::int32_t*>(record + 0x3C),
						*reinterpret_cast<const std::uint64_t*>(record + 0x18),
						*reinterpret_cast<void* const*>(record + 0x20));
				}
				console::info("[HQ contracts] nine retail periodic rows: AEC_CONTRACT, StatsTable contract cost tokens, match-only timers, expiration 0\n");
			}
			catch (const std::exception& error)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] contract_state: %s\n", error.what());
			}
			catch (...)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] contract_state: unknown exception\n");
			}
		}
		void ownership_status(const command::params& params)
		{
			std::string_view value = params.size() == 2 ? params[1] : "";
			const auto hex = value.starts_with("0x") || value.starts_with("0X");
			if (hex) value.remove_prefix(2);
			unsigned guid{};
			const auto parsed = std::from_chars(value.data(), value.data() + value.size(), guid, hex ? 16 : 10);
			if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !guid)
			{
				console::info("Usage: hqownership <decimal|0xGUID> (read-only native/CAC ownership)\n");
				return;
			}
			scheduler::once([guid]
			{
				static std::atomic_bool warned{};
				try
				{
					if (!*reinterpret_cast<const unsigned char*>(0x80385A8_g))
					{
						console::info("[HQ ownership] inventory not ready\n");
						return;
					}
					const demonware::hq_inventory_cache::record* cached{};
					utils::hook::invoke<unsigned short>(0x279300_g, 0, guid, &cached);
					const auto quantity = utils::hook::invoke<unsigned>(0x279480_g, 0, guid);
					const auto usable = utils::hook::invoke<bool>(0x27A310_g, 0, guid);
					const auto lock = utils::hook::invoke<int>(0xCF850_g, 0, guid, nullptr);
					console::info("[HQ ownership] guid=0x%x cached=%u quantity=%u expires=%u duration=%lld usable=%u lock=%d (0=unlocked)\n",
						guid, cached ? cached->quantity : 0, quantity, cached ? cached->expires : 0,
						cached ? cached->duration : 0, unsigned(usable), lock);
					demonware::hq_protocol::trace("ownership_native", std::to_string(guid) + ":quantity=" +
						std::to_string(quantity) + ":usable=" + std::to_string(usable) + ":lock=" + std::to_string(lock));
					if (!*game::hks::lui_lua_state) return;
					const auto lua = ui_scripting::get_globals();
					const std::string key = utils::string::va("0x%x", guid);
					const auto unlocked = lua["Engine"]["IsGuidUnlocked"](0, key);
					const auto cac = lua["Cac"]["GetItemGuidLockState"](0, key);
					console::info("[HQ ownership] IsGuidUnlocked=%u CAC=%s\n", unsigned(unlocked.at(0).as<bool>()),
						cac.at(0).as<std::string>().c_str());
				}
				catch (const std::exception& error)
				{
					demonware::hq_logging::safe_warn_once(warned, "[HQ callback] ownership probe: %s\n", error.what());
				}
				catch (...)
				{
					demonware::hq_logging::safe_warn_once(warned, "[HQ callback] ownership probe: unknown exception\n");
				}
			}, scheduler::pipeline::main);
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
			command::add("hqnative", [] { status(); });
			command::add("hqwallet", wallet_status);
			scheduler::loop(sync_wallet, scheduler::pipeline::main, 100ms);
			scheduler::loop(sync_inventory, scheduler::pipeline::main, 100ms);
			command::add("hqvendor", vendor_status_command);
			scheduler::loop(watchdog_tick, scheduler::pipeline::main, 100ms);
			command::add("hqmail", mail_status);
			command::add("hqopendrop", open_drop);
			command::add("hqtask99", task99);
			command::add("hqskutest", sku_test);
			command::add("hqpayrollstate", payroll_state);
			command::add("hqcontracts", [] { scheduler::once(contract_state, scheduler::pipeline::main); });
			command::add("hqownership", ownership_status);
		}
	};
}

REGISTER_COMPONENT(hq_native::component)
