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
		class ae_result final : public bdTaskResult
		{
		public:
			std::string json{};
			void serialize(byte_buffer* buffer) override
			{
				// Provisional mirrored task 4/5 envelope; see slice report.
				buffer->write_string("s2_steam");
				buffer->write_uint16(1);
				buffer->write_int32(1);
				buffer->write_string(json);
			}
		};

		void dispatch_ae(service_server* server, byte_buffer* buffer, const std::uint8_t task)
		{
			hq_protocol::trace("reward_request", buffer->get_remaining());
			std::string context{}, json{};
			unsigned short count{};
			int type{};
			if (buffer->size() > 65536 || !buffer->read_string(&context) || context != "s2_steam" ||
				!buffer->read_uint16(&count) || count != 1 || !buffer->read_int32(&type) || type != 1 ||
				!buffer->read_string(&json) || !hq_protocol::padding(buffer))
			{
				console::warn("[HQ AE] invalid bdReward task %u framing\n", task);
				server->create_reply(task, BD_REWARD_EVENTS_DATA_ERROR).send();
				return;
			}
			auto result = std::make_unique<ae_result>();
			result->json = achievement_engine::dispatch(json);
			byte_buffer raw{};
			result->serialize(&raw);
			hq_protocol::trace("reward_reply", raw.get_buffer());
			auto reply = server->create_reply(task);
			reply.add(result);
			reply.send_struct();
		}

		void submit_hidden_challenge_events(std::vector<reward_game_events::event>& events)
		{
			for (auto& event : events)
			{
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
