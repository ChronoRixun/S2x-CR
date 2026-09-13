#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include <utils/hook.hpp>
#include "game/demonware/hq_vendor.hpp"

namespace hq_native
{
	namespace
	{
		utils::hook::detour sku_success_hook;
		utils::hook::detour sku_failure_hook;
		utils::hook::detour conversion_success_hook;
		utils::hook::detour conversion_failure_hook;
		unsigned conversion_successes{}, conversion_failures{};

		void status()
		{
			console::info("[HQ native] SKUs fetched=%u (raw flag); use aecache for Orders\n",
				*reinterpret_cast<const unsigned char*>(0x81038A8_g));
		}

		void vendor_status()
		{
			status();
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
				conversion_successes, conversion_failures, 24, reinterpret_cast<const char*>(0x81960C0_g),
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
			const auto ready = *reinterpret_cast<const int*>(0x8A14F84_g);
			const auto* slots = *reinterpret_cast<const unsigned char* const*>(0x8A15010_g);
			console::info("[HQ mail native] ready=%d slots=%p; first 14 advertised slots\n", ready, slots);
			if (!ready || !slots) return;
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
			sku_failure_hook.create(0x27B6C0_g, sku_failure);
			conversion_success_hook.create(0x27A4C0_g, conversion_success);
			conversion_failure_hook.create(0x27A460_g, conversion_failure);
			command::add("hqnative", status);
			command::add("hqvendor", vendor_status);
			command::add("hqmail", mail_status);
			command::add("hqopendrop", open_drop);
		}
	};
}

REGISTER_COMPONENT(hq_native::component)
