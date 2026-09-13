#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include <utils/hook.hpp>
#include "game/demonware/hq_vendor.hpp"
#include "game/demonware/hq_payroll.hpp"
#include "game/demonware/hq_mail.hpp"
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
				std::optional<std::string> notification;
				{
					std::lock_guard lock{demonware::hq_payroll::notification_mutex};
					notification.swap(demonware::hq_payroll::notification);
				}
				if (notification)
				{
					auto* bridge = game::AE_UserAchievementTaskData.get() + 0xF8;
					if (game::AE_SetResponseString(bridge, notification->c_str()))
					{
						demonware::hq_protocol::trace("payroll_native_push", *notification);
						// 13C480 is the achievement push handler, distinct from task replies.
						utils::hook::invoke<void>(0x13C480_g, 0, bridge);
						console::info("[HQ payroll] delivered persisted completion push\n");
					}
					else
					{
						std::lock_guard lock{demonware::hq_payroll::notification_mutex};
						if (!demonware::hq_payroll::notification) demonware::hq_payroll::notification = std::move(notification);
					}
				}
				const auto data = demonware::hq_economy::snapshot();
				for (const auto& [id, amount] : data.currencies)
				{
					if (!id || utils::hook::invoke<unsigned>(0x279780_g, 0, unsigned(id)) == amount) continue;
					// Native absolute setter + inventory eventType 5; no second grant.
					utils::hook::invoke<void>(0x27D510_g, 0, unsigned(id), amount);
				}
			}
			catch (const std::exception& error)
			{
				static bool warned{};
				if (!std::exchange(warned, true)) console::warn("[HQ wallet] sync failed: %s\n", error.what());
			}
		}

		void status()
		{
			console::info("[HQ native] SKUs fetched=%u (raw flag); use aecache for Orders\n",
				*reinterpret_cast<const unsigned char*>(0x81038A8_g));
		}

		void reject_purchase(unsigned, unsigned sku, unsigned, void* transaction, int)
		{
			// 276580 initializes this caller-owned bdString before validating a purchase.
			// A null string returns Lua nil in 11FDF0, without creating a native task.
			if (transaction) std::memset(transaction, 0, 25);
			console::warn("[HQ vendor] SKU %u purchase unavailable (local display catalog)\n", sku);
		}

		void vendor_status()
		{
			status();
			wallet_status();
			unsigned sku_count{};
			for (unsigned i = 0; i < 400; ++i)
			{
				const auto* sku = reinterpret_cast<const unsigned char*>(0x81038B0_g) + i * 0x2E8;
				if (!*reinterpret_cast<const unsigned*>(sku)) continue;
				++sku_count;
				console::info("[HQ vendor] SKU slot=%u id=%u type=%u max=%u prices=%u product=%u items=%u\n", i,
					*reinterpret_cast<const unsigned*>(sku), *reinterpret_cast<const unsigned*>(sku + 4),
					*reinterpret_cast<const unsigned*>(sku + 12), sku[0x2E1],
					*reinterpret_cast<const unsigned*>(sku + 0x240), sku[0x244]);
			}
			console::info("[HQ vendor] catalogType=%u nonzeroSKUs=%u inventoryAndBalanceReady=%u\n",
				*reinterpret_cast<const unsigned*>(0x81038AC_g), sku_count,
				utils::hook::invoke<bool>(0x27A210_g, 0));
			for (const auto offset : vendor_globals)
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
			console::info("[HQ vendor] conversion successes=%u failures=%u responseTx=%.*s scalar=%llu currencyCount=%u inventoryCount=%u extraCount=%u\n",
				conversion_successes.load(), conversion_failures.load(), 24, reinterpret_cast<const char*>(0x81960C0_g),
				*reinterpret_cast<const std::uint64_t*>(0x81960E0_g),
				*reinterpret_cast<const unsigned*>(0x8196254_g), *reinterpret_cast<const unsigned*>(0x8196264_g),
				*reinterpret_cast<const unsigned*>(0x8196274_g));
			console::info("[HQ vendor] entitlement fetched flag and final LUI enable expression unresolved\n");
		}

		void conversion_success(void* task)
		{
			conversion_success_hook.invoke<void>(task);
			++conversion_successes;
			console::info("[HQ vendor] conversion-rule native success callback\n");
			vendor_status();
		}

		void conversion_failure(void* task)
		{
			conversion_failure_hook.invoke<void>(task);
			++conversion_failures;
			console::warn("[HQ vendor] conversion-rule native failure callback\n");
			vendor_status();
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
			purchase_hook.create(0x276580_g, reject_purchase);
			sku_failure_hook.create(0x27B6C0_g, sku_failure);
			conversion_success_hook.create(0x27A4C0_g, conversion_success);
			conversion_failure_hook.create(0x27A460_g, conversion_failure);
			command::add("hqnative", status);
			command::add("hqwallet", wallet_status);
			scheduler::loop(sync_wallet, scheduler::pipeline::main, 100ms);
			command::add("hqvendor", vendor_status);
			command::add("hqmail", mail_status);
			command::add("hqopendrop", open_drop);
		}
	};
}

REGISTER_COMPONENT(hq_native::component)
