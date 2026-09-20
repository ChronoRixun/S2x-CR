#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "achievement_sync.hpp"
#include "component/scheduler.hpp"
#include "game/game.hpp"

namespace achievement_sync
{
	namespace
	{
		std::atomic_bool accepting_refresh_requests{};
		std::atomic_bool refresh_pending{};
		bool lobby_was_loaded{};

		void refresh_when_ready()
		{
			if (!accepting_refresh_requests.load()) return;
			const auto lobby = game::virtual_lobby_loaded();
			if (lobby && !lobby_was_loaded) refresh_pending = true;
			lobby_was_loaded = lobby;
			if (!lobby || !refresh_pending.load() || !game::AE_GetUserContext(0)) return;
			// The synchronous injection hook owns all replies, including the legacy
			// hidden achievements. Never overwrite its merged response with a second
			// legacy-only response after the native fetch has returned.
			const auto* active = reinterpret_cast<const unsigned*>(game::AE_UserAchievementTaskData.get() + 0xC0);
			if (*active) return;
			if (!refresh_pending.exchange(false)) return;
			char transaction[32]{};
			game::AE_GenerateTransactionId(transaction);
			if (!game::AE_FetchUserAchievementsByPage(0, "", transaction, 0)) refresh_pending = true;
		}
	}

	void request_refresh()
	{
		if (accepting_refresh_requests.load()) refresh_pending = true;
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated() || !game::environment::is_zombies()) return;
			accepting_refresh_requests = true;
			scheduler::loop(refresh_when_ready, scheduler::pipeline::main, 100ms);
		}

		void pre_destroy() override
		{
			accepting_refresh_requests = false;
			refresh_pending = false;
		}
	};
}

REGISTER_COMPONENT(achievement_sync::component)
