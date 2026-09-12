#include <std_include.hpp>
#include "../dw_include.hpp"

#include "component/console/console.hpp"
#include "component/hidden_challenge_relay.hpp"
#include "component/hidden_challenges.hpp"

#include "game/game.hpp"
#include "../achievement_engine.hpp"
#include "../hq_protocol.hpp"
#include "game/demonware/reward_game_event.hpp"

#include "steam/steam.hpp"

namespace demonware
{
	namespace
	{
		void dispatch_ae(service_server* server, byte_buffer* buffer, const std::uint8_t task)
		{
			hq_protocol::trace("reward_request", buffer->get_remaining());
			std::string json{};
			if (!hq_protocol::parse_ae(buffer, json))
			{
				console::warn("[HQ AE] invalid bdReward task %u framing\n", task);
				server->create_reply(task, BD_REWARD_EVENTS_DATA_ERROR).send();
				return;
			}
			// AE JSON is delivered synchronously by the native submit hook. Dispatching
			// again here would apply mutations twice and structured replies fail tasks.
			server->create_reply(task).send();
		}

		void submit_hq_event(const reward_game_events::event& event)
		{
			if (game::environment::is_zombies() || game::environment::is_dedicated()) return;
			if (utils::flags::has_flag("-demonware_debug"))
			{
				const auto* name = event.name == "1" ? "killed_a_player" :
					event.name == "18" ? "picked_up_payroll" : event.name.c_str();
				console::info("[HQ event] %s (%s), timestamp %lld, %zu parameters\n",
					event.name.c_str(), name, event.timestamp, event.parameters.size());
				for (const auto& parameter : event.parameters)
					console::info("  %s=%llu\n", parameter.selector.c_str(), parameter.value);
			}
			if (!achievement_engine::submit_event(event)) console::warn("[HQ event] economy update failed\n");
		}

		void submit_hidden_challenge_events(std::vector<reward_game_events::event>& events)
		{
			for (auto& event : events)
			{
				submit_hq_event(event);
				hidden_challenges::submit_reward_game_event(std::move(event));
			}
		}
	}

	bdReward::bdReward() : service(139, "bdReward")
	{
		this->register_task(1, &bdReward::incrementTime);
		this->register_task(2, &bdReward::claimRewardRoll);
		this->register_task(3, &bdReward::claimClientAchievements);
		this->register_task(4, &bdReward::reportRewardEvents);
		this->register_task(5, &bdReward::reportRewardEventsSync);

		this->register_task(11, &bdReward::reportRewardGameEventsForUsers);
		this->register_task(12, &bdReward::reportRewardGameEvents);
	}

	void bdReward::incrementTime(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdReward::claimRewardRoll(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdReward::claimClientAchievements(service_server* server, byte_buffer* /*buffer*/) const
	{
		// TODO:
		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdReward::reportRewardEvents(service_server* server, byte_buffer* buffer) const
	{
		dispatch_ae(server, buffer, this->task_id());
	}

	void bdReward::reportRewardGameEventsForUsers(service_server* server, byte_buffer* buffer) const
	{
		std::vector<reward_game_events::user_event_batch> users{};
		if (reward_game_events::parse_report_for_users_request(buffer, users))
		{
			const auto dedicated = game::environment::is_dedicated();
			const auto local_user_id = dedicated ? 0 : steam::SteamUser()->GetSteamID().bits;
			for (auto& user : users)
			{
				if (user.account_type != "steam")
				{
					continue;
				}

				for (auto& event : user.events)
				{
					if (!dedicated && user.user_id == local_user_id) submit_hq_event(event);
					std::uint32_t group{};
					std::uint32_t challenge{};
					if (!hidden_challenges::get_completion(event, group, challenge))
					{
						continue;
					}

					console::debug(
						"[hidden_challenges] task11 XUID %llu: zombies [3=%u, 4=%u]\n",
						static_cast<unsigned long long>(user.user_id), group, challenge);
					if (!dedicated && user.user_id == local_user_id)
					{
						hidden_challenges::submit_reward_game_event(std::move(event));
					}
					else
					{
						hidden_challenge_relay::submit(user.user_id, group, challenge);
					}
				}
			}
		}
		else
		{
			console::debug("[hidden_challenges] ignored a malformed bdReward task 11 request\n");
		}

		auto reply = server->create_reply(this->task_id());
		reply.send();
	}

	void bdReward::reportRewardEventsSync(service_server* server, byte_buffer* buffer) const
	{
		dispatch_ae(server, buffer, this->task_id());
	}

	void bdReward::reportRewardGameEvents(service_server* server, byte_buffer* buffer) const
	{
		std::vector<reward_game_events::event> events{};
		if (reward_game_events::parse_report_request(buffer, events))
		{
			submit_hidden_challenge_events(events);
		}
		else
		{
			console::debug("[hidden_challenges] ignored a malformed bdReward task 12 request\n");
		}

		auto reply = server->create_reply(this->task_id());
		reply.send();
	}
}
