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
		}
	};
}

REGISTER_COMPONENT(hq_native::component)
