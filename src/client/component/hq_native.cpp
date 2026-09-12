#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "component/command.hpp"
#include "component/console/console.hpp"
#include <utils/hook.hpp>

namespace hq_native
{
	namespace
	{
		utils::hook::detour sku_success_hook;
		utils::hook::detour sku_failure_hook;

		void status()
		{
			console::info("[HQ native] SKUs fetched=%u (raw flag); use aecache for Orders\n",
				*reinterpret_cast<const unsigned char*>(0x81038A8_g));
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
			command::add("hqnative", status);
			command::add("hqopendrop", open_drop);
		}
	};
}

REGISTER_COMPONENT(hq_native::component)
