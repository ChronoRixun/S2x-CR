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

	}

	bool submit_hq_event(const reward_game_events::event& event)
	{
		if (game::environment::is_zombies() || game::environment::is_dedicated()) return true;
		if (utils::flags::has_flag("-demonware_debug"))
		{
			// Names from build/research/tables/dwgameevents.csv.
			static const std::map<std::string, std::string> names
			{
				{"1", "killed_a_player"},
				{"2", "multi_kill"},
				{"3", "gamemode_action"},
				{"4", "streak"},
				{"5", "end_game"},
				{"6", "flak_gun_event"},
				{"7", "one_v_one"},
				{"8", "firing_range"},
				{"9", "shootout"},
				{"10", "social"},
				{"11", "vendor"},
				{"12", "supply_drop"},
				{"13", "enter_hub"},
				{"14", "player_rank_up"},
				{"15", "division_rank_up"},
				{"16", "zombies"},
				{"17", "redeemed_challenge"},
				{"18", "picked_up_payroll"},
				{"20", "equippedSomethingInCAC"},
				{"21", "completed_hq_onboard_phase1"},
				{"22", "completed_hq_onboard_phase2"},
				{"23", "completed_hq_onboard_phase3"},
				{"24", "enter_scorestreak_training"},
				{"25", "picked_up_orders"},
				{"26", "completed_hub_fte"},
				{"27", "social_score"},
				{"28", "special_unlock"},
				{"29", "_game_pump_mp"},
				{"30", "_game_pump_zombies"},
				{"31", "ranked_play_advance"},
				{"32", "assists"},
				{"33", "ugc_vote"},
				{"34", "zombies_kills"},
				{"35", "zombies_multikill"},
				{"36", "zombies_jolts"},
				{"37", "zombies_waves"},
				{"38", "zombies_special"},
				{"39", "zombies_st_patrick"},
				{"40", "killed_a_zombie"},
				{"41", "zombies_map_won"},
				{"42", "zombies_dlc3_sv_unlock"},
				{"43", "zombies_dlc3_ee_unlock"},
				{"44", "zombies_dlc3_skull_unlock"},
				{"45", "zombies_skin_unlock"},
				{"46", "store"},
			};
			const auto found = names.find(event.name);
			const auto* name = found == names.end() ? event.name.c_str() : found->second.c_str();
			console::info("[HQ event] %s (%s), timestamp %lld, %zu parameters\n",
				event.name.c_str(), name, event.timestamp, event.parameters.size());
			for (const auto& parameter : event.parameters)
				console::info("  %s=%llu\n", parameter.selector.c_str(), parameter.value);
		}
		const auto ok = achievement_engine::submit_event(event, true);
		if (!ok) console::warn("[HQ event] economy update failed\n");
		return ok;
	}

	namespace
	{
		bool submit_hidden_challenge_events(std::vector<reward_game_events::event>& events)
		{
			bool ok = true;
			for (auto& event : events)
			{
				ok = submit_hq_event(event) && ok;
				hidden_challenges::submit_reward_game_event(std::move(event));
			}
			return ok;
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
		hq_protocol::trace("reward_11", buffer->get_remaining());
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
					if (!game::environment::is_zombies())
					{
						if (!dedicated && user.user_id == local_user_id) submit_hq_event(event);
						else hidden_challenge_relay::submit_reward(user.user_id, event);
					}
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

		// Struct task (typed 0x17 request payload): the SDK only completes it on a
		// structured reply. A count-framed acknowledgement fails the task client-side and
		// the event queue re-sends the same batch with exponential backoff (run-25248:
		// enter_hub at transactions 184, 341, 658, 1289).
		auto reply = server->create_reply(this->task_id());
		auto body = std::make_unique<hq_protocol::empty_struct_result>();
		reply.add(body);
		reply.send_struct();
	}

	void bdReward::reportRewardEventsSync(service_server* server, byte_buffer* buffer) const
	{
		dispatch_ae(server, buffer, this->task_id());
	}

	void bdReward::reportRewardGameEvents(service_server* server, byte_buffer* buffer) const
	{
		hq_protocol::trace("reward_12", buffer->get_remaining());
		bool ok = true;
		std::vector<reward_game_events::event> events{};
		if (reward_game_events::parse_report_request(buffer, events))
		{
			ok = submit_hidden_challenge_events(events);
		}
		else
		{
			console::debug("[hidden_challenges] ignored a malformed bdReward task 12 request\n");
		}

		// Same struct-task framing as task 11. Replayed events (the kiosk re-sends
		// picked_up_payroll until the task succeeds) are idempotent in the economy store
		// (hq_payroll::settle acknowledges an already settled period without a grant), so
		// the batch only fails when the store itself could not be updated.
		if (!ok)
		{
			server->create_reply(this->task_id(), BD_HANDLE_TASK_FAILED).send_struct();
			return;
		}
		auto reply = server->create_reply(this->task_id());
		auto body = std::make_unique<hq_protocol::empty_struct_result>();
		reply.add(body);
		reply.send_struct();
	}
}
