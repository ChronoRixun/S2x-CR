#include <std_include.hpp>
#include "game/demonware/hq_logging.hpp"
#include "loader/component_loader.hpp"

#include "hidden_challenge_relay.hpp"

#include "command.hpp"
#include "console/console.hpp"
#include "hidden_challenges.hpp"
#include "scheduler.hpp"

#include "game/game.hpp"
#include "game/demonware/hq_event_relay.hpp"
#include "game/demonware/hq_relay_queue.hpp"
#include "game/demonware/achievement_engine.hpp"
#include "game/demonware/hq_protocol.hpp"
#include "steam/steam.hpp"

#include <charconv>
#include <chrono>
#include <deque>
#include <mutex>
#include <map>

#include <utils/hook.hpp>

namespace hidden_challenge_relay
{
	namespace
	{
		constexpr std::string_view server_command = "s2x_hc";
		constexpr auto maximum_pending_forwards = 128u;
		// Two per XUID leave room for all 48 party members within the global cap,
		// even when connected clients wait indefinitely below the send threshold.
		constexpr auto maximum_forwards_per_client = 2u;
		constexpr auto minimum_command_client_state = 5;
		// Four fragments per client/frame smooth bursts; 32 outstanding leaves 96
		// of the engine's 128 reliable slots available for ordinary game traffic.
		constexpr unsigned fragments_per_client_frame = 4, maximum_relay_backlog = 32;

		struct pending_forward
		{
			std::uint64_t user_id{};
			std::uint32_t group{};
			std::uint32_t challenge{};
			std::string reward_command{};
		};

		utils::hook::detour deploy_server_command_hook;
		std::atomic_bool accepting_forwards{};
		std::mutex pending_forward_mutex{};
		std::deque<pending_forward> pending_forwards{};

		demonware::hq_event_relay::client_queue client_events;
		demonware::hq_event_relay::server_queue server_events;

		void warn_client_overflow()
		{
			static std::uint64_t next_warning{};
			const auto now = GetTickCount64();
			if (now >= next_warning)
			{
				next_warning = now + 5000;
				console::warn("[HQ relay] client queue full; dropping newest event\n");
			}
		}

		demonware::hq_event_relay::receiver& client_receiver()
		{
			static demonware::hq_event_relay::receiver receiver;
			return receiver;
		}

		bool queue_client_event(const demonware::reward_game_events::event& event)
		{
			if (!demonware::achievement_engine::valid_event(event, true))
			{
				static std::uint64_t next_rejection_warning{};
				const auto now = GetTickCount64();
				if (now >= next_rejection_warning)
				{
					next_rejection_warning = now + 5000;
					console::warn("[HQ relay] rejected invalid server event\n");
				}
				return false;
			}
			if (client_events.push(event)) return true;
			warn_client_overflow();
			return false;
		}

		void process_client_events()
		{
			static std::atomic_bool warned{};
			try
			{
				// Only the async worker owns the in-flight batch. A failed transaction
				// keeps it ahead of later events; queue producers never wait on store I/O.
				static std::vector<demonware::reward_game_events::event> batch;
				if (!accepting_forwards.load()) return;
				if (batch.empty()) batch = client_events.take();
				if (batch.empty()) return;
				if (!demonware::achievement_engine::submit_relay_events(batch)) return;
				const auto count = batch.size();
				batch.clear(); // A diagnostic failure must not replay a committed batch.
				if (utils::flags::has_flag("-demonware_debug"))
					console::info("[HQ relay] applied %zu queued events\n", count);
			}
			catch (const std::exception& error)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] process_client_events: %s\n", error.what());
			}
			catch (...)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] process_client_events: unknown exception\n");
			}
		}

		bool parse_unsigned(const char* text, std::uint32_t& value)
		{
			if (!text || !*text)
			{
				return false;
			}

			const auto* end = text + std::strlen(text);
			const auto result = std::from_chars(text, end, value);
			return result.ec == std::errc{} && result.ptr == end;
		}

		void deploy_server_command_stub(const unsigned int local_client_num)
		{
			// CG_DeployServerCommandString is entered with the reliable command
			// already tokenized by the stock client command path.
			const command::params params{};
			if (params.size() && std::string_view{params[0]} == demonware::hq_event_relay::command)
			{
				if (game::environment::is_zombies() || local_client_num != 0 || params.size() != 7) return;
				try
				{
					// Drop before reconstruction/reassembly/decoding when no slot is free.
					if (client_events.full())
					{
						warn_client_overflow();
						return;
					}
					std::string wire;
					for (auto i = 0; i < params.size(); ++i)
					{
						const auto length = strnlen(params[i], demonware::hq_event_relay::maximum_command + 1);
						if (wire.size() + length + (i ? 1 : 0) > demonware::hq_event_relay::maximum_command) return;
						if (i) wire += ' ';
						wire.append(params[i], length);
					}
					const auto user = steam::SteamUser()->GetSteamID().bits;
					client_receiver().accept(wire, user, queue_client_event);
				}
				catch (...) { console::warn("[HQ relay] could not apply server event\n"); }
				return;
			}
			if (params.size() == 0 || std::string_view{params[0]} != server_command)
			{
				deploy_server_command_hook.invoke<void>(local_client_num);
				return;
			}

			if (!game::environment::is_zombies()) return;
			std::uint32_t group{};
			std::uint32_t challenge{};
			if (params.size() != 3 || !parse_unsigned(params[1], group) ||
				!parse_unsigned(params[2], challenge))
			{
				console::debug("[hidden_challenges] ignored malformed server completion\n");
				return;
			}

			console::debug("[hidden_challenges] received server completion: group=%u slot=%u\n",
				group, challenge);
			hidden_challenges::submit_completion(group, challenge);
		}

		void process_pending_forwards()
		{
			static std::atomic_bool warned{};
			try
			{
				if (!game::SV_Loaded())
				{
					return;
				}

				auto* party = game::Live_GetGameParty();
				auto* clients = *game::mp::svs_clients;
				const auto max_clients = *game::sv_maxclients;
				if (!party || !clients || max_clients <= 0)
				{
					return;
				}

				std::deque<pending_forward> forwards{};
				{
					std::lock_guard lock{pending_forward_mutex};
					if (game::environment::is_zombies())
					{
						for (auto it = pending_forwards.begin(); it != pending_forwards.end();)
						{
							const auto client_num = game::Party_FindMemberByXUID(party, it->user_id);
							// Connected clients (state >= 3) keep their queue slots until active.
							if (client_num != std::numeric_limits<std::uint8_t>::max() && client_num < max_clients &&
								clients[client_num].state >= 3 && clients[client_num].state < minimum_command_client_state)
							{
								++it;
								continue;
							}
							forwards.push_back(std::move(*it));
							it = pending_forwards.erase(it);
						}
					}
				}

				if (!game::environment::is_zombies())
				{
					std::map<unsigned, unsigned> scheduled;
					// The party has at most 48 members: every eligible client gets its budget.
					auto parts = server_events.take(48 * fragments_per_client_frame, [&](const std::uint64_t user)
					{
						const auto client_num = game::Party_FindMemberByXUID(party, user);
						if (client_num == std::numeric_limits<std::uint8_t>::max() || client_num >= max_clients)
							return false;
						const auto& client = clients[client_num];
						if (client.state < minimum_command_client_state) return false;
						const auto outstanding = client.reliableSequence - client.reliableAcknowledge;
						auto& count = scheduled[client_num];
						if (count >= fragments_per_client_frame || outstanding >= maximum_relay_backlog ||
							count >= maximum_relay_backlog - outstanding) return false;
						++count; // Include fragments selected here but not yet sent to the engine.
						return true;
					});
					// Preserve selection order through SV_CMD_RELIABLE: no per-client event interleaving.
					for (auto& [user, part] : parts)
						forwards.push_back({user, 0, 0, std::move(part)});
				}

				for (const auto& forward : forwards)
				{
					// The stock Achievement Engine sender resolves its XUID through this
					// party lookup and uses the returned member as the svs_clients index.
					const auto client_num = game::Party_FindMemberByXUID(party, forward.user_id);
					if (client_num == std::numeric_limits<std::uint8_t>::max() ||
						client_num >= max_clients || clients[client_num].state < minimum_command_client_state)
					{
						try { console::debug("[hidden_challenges] discarded completion for disconnected XUID %llu\n",
							static_cast<unsigned long long>(forward.user_id)); }
						catch (...) {} // Keep sending the remaining forwards.
						continue;
					}

					if (!forward.reward_command.empty())
					{
						game::SV_SendServerCommand(&clients[client_num], game::SV_CMD_RELIABLE,
							"%s", forward.reward_command.c_str());
						try { demonware::hq_protocol::trace("relay_forwarded", forward.reward_command); }
						catch (...) {} // Keep sending the remaining forwards.
						continue;
					}
					try { console::debug(
						"[hidden_challenges] forwarding XUID %llu to client %u: group=%u slot=%u\n",
						static_cast<unsigned long long>(forward.user_id), client_num,
						forward.group, forward.challenge); }
					catch (...) {} // Keep sending the remaining forwards.
					game::SV_SendServerCommand(&clients[client_num], game::SV_CMD_RELIABLE,
						"%s %u %u", server_command.data(), forward.group, forward.challenge);
				}
			}
			catch (const std::exception& error)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] process_pending_forwards: %s\n", error.what());
			}
			catch (...)
			{
				demonware::hq_logging::safe_warn_once(warned, "[HQ callback] process_pending_forwards: unknown exception\n");
			}
		}

		// Party_FindMemberByXUID (0x6FDDA0) walks 48 entries of a 0x38-byte table inside
		// PartyData: a presence byte at +0xC0 + i * 0x38 and the member XUID at
		// +0x90 + i * 0x38 (it returns 0xFF when no entry matches). Enumerating with the
		// same layout means `hqrelaytest all` can only address members the relay itself
		// could address; nothing here writes to the table.
		constexpr std::size_t party_member_stride = 0x38, party_member_count = 48;
		constexpr std::size_t party_member_xuid = 0x90, party_member_present = 0xC0;

		std::vector<std::uint64_t> party_xuids()
		{
			std::vector<std::uint64_t> found{};
			const auto* party = reinterpret_cast<const unsigned char*>(game::Live_GetGameParty());
			if (!party) return found;
			for (std::size_t i = 0; i < party_member_count; ++i)
			{
				if (!party[party_member_present + i * party_member_stride]) continue;
				std::uint64_t xuid{};
				std::memcpy(&xuid, party + party_member_xuid + i * party_member_stride, sizeof(xuid));
				if (xuid) found.push_back(xuid);
			}
			return found;
		}

		// A real dedicated killed_a_player carries a long parameter vector; slice 8 fixed
		// 150 parameters as the tested shape and selector 6 = 1 as the headshot flag (the
		// "(6:1)" definitions are daily_ch_headshots and contract_mp_2). Every selector is
		// present, valued 0, so the flag-word predicate (130:4)&&(130:128) and the
		// equipment OR (1:8)||(1:9) correctly do not match a plain kill.
		constexpr unsigned synthetic_kill_parameters = 150, headshot_selector = 6;
		constexpr std::uint32_t maximum_synthetic_kills = 64;

		demonware::reward_game_events::event synthetic_event(const char* name, const std::int64_t timestamp,
			const std::initializer_list<std::pair<unsigned, std::uint64_t>> parameters)
		{
			demonware::reward_game_events::event event{};
			event.name = name;
			event.timestamp = timestamp;
			for (const auto& [selector, value] : parameters)
				event.parameters.push_back({std::to_string(selector), value});
			return event;
		}

		demonware::reward_game_events::event synthetic_kill(const std::int64_t timestamp, const bool headshot)
		{
			demonware::reward_game_events::event event{};
			event.name = "killed_a_player";
			event.timestamp = timestamp;
			for (unsigned selector = 1; selector <= synthetic_kill_parameters; ++selector)
				event.parameters.push_back({std::to_string(selector),
					selector == headshot_selector && headshot ? 1ull : 0ull});
			return event;
		}

		// hqrelaytest <xuid|all> [kills] [headshots]. Intended for the DEDICATED server
		// console: it builds a task-11 shaped batch for one user and hands each event to
		// route_reward_user_event, the same function the live bdReward task-11 handler
		// calls, so the events reach the owning client over the ordinary chunked relay.
		void relay_test_command(const command::params& params)
		{
			std::uint64_t requested_xuid{};
			std::uint32_t kills = 1, headshots = 0;
			if (params.size() < 2 || params.size() > 4 ||
				(std::string_view{params[1]} != "all" &&
					(!demonware::hq_event_relay::number(std::string_view{params[1]}, requested_xuid) || !requested_xuid)) ||
				(params.size() > 2 && !parse_unsigned(params[2], kills)) ||
				(params.size() > 3 && !parse_unsigned(params[3], headshots)))
			{
				console::info("[HQ relay test] usage: hqrelaytest <xuid|all> [kills] [headshots] (sends synthetic reward events to connected clients; requires sv_cheats 1)\n");
				return;
			}
			if (game::environment::is_zombies())
			{
				console::info("[HQ relay test] multiplayer only\n");
				return;
			}
			// Match the existing multiplayer developer commands' sv_cheats gate.
			const auto* cheats = game::Dvar_FindMalleableVar("sv_cheats");
			if (!cheats || !cheats->current.enabled)
			{
				console::info("[HQ relay test] requires sv_cheats 1; sends synthetic reward events to connected clients\n");
				return;
			}
			const auto dedicated = game::environment::is_dedicated();
			const auto local = dedicated ? 0ull : steam::SteamUser()->GetSteamID().bits;
			const auto members = party_xuids();
			std::vector<std::uint64_t> targets{};
			if (requested_xuid) targets.push_back(requested_xuid);
			else
			{
				targets = members;
				if (targets.empty() && local) targets.push_back(local);
			}
			if (targets.empty())
			{
				console::info("[HQ relay test] no addressable member; pass an explicit XUID\n");
				return;
			}
			console::info("[HQ relay test] running on a %s; %zu party members, %zu targets\n",
				dedicated ? "dedicated server" : "client", members.size(), targets.size());
			if (kills > maximum_synthetic_kills) kills = maximum_synthetic_kills;
			if (headshots > kills) headshots = kills;

			auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
			for (std::size_t slot = 0; slot < targets.size(); ++slot)
			{
				const auto xuid = targets[slot];
				// Distinct timestamps: the store's replay receipt is (event id, timestamp,
				// sorted parameters), so otherwise identical kills would collapse into one.
				unsigned failed{};
				for (std::uint32_t i = 0; i < kills; ++i)
				{
					auto event = synthetic_kill(stamp++, i < headshots);
					if (demonware::route_reward_user_event(xuid, event) == demonware::reward_delivery::retryable_failure) ++failed;
				}
				auto multi = synthetic_event("multi_kill", stamp++, {{1, 4}, {2, 1}, {3, 2}});
				if (demonware::route_reward_user_event(xuid, multi) == demonware::reward_delivery::retryable_failure) ++failed;
				auto win = synthetic_event("end_game", stamp++, {{1, 1}, {2, 1}});
				if (demonware::route_reward_user_event(xuid, win) == demonware::reward_delivery::retryable_failure) ++failed;
				console::info("[HQ relay test] target slot %zu: %u killed_a_player (%u with selector 6 = 1), 1 multi_kill, 1 end_game\n",
					slot, kills, headshots);
				console::info("[HQ relay test] target slot %zu: %u accepted, %u retryable failures\n",
					slot, kills + 2 - failed, failed);
			}
		}

		void clear_pending_forwards()
		{
			std::lock_guard lock{pending_forward_mutex};
			pending_forwards.clear();
			server_events.clear();
		}
	}

	void submit(const std::uint64_t user_id, const std::uint32_t group,
		const std::uint32_t challenge)
	{
		if (!accepting_forwards.load())
		{
			return;
		}

		std::lock_guard lock{pending_forward_mutex};
		if (!accepting_forwards.load())
		{
			return;
		}

		// Bound at insertion so a parked client cannot fill the shared queue between frames.
		const auto belongs_to_client = [user_id](const pending_forward& forward) { return forward.user_id == user_id; };
		if (std::count_if(pending_forwards.begin(), pending_forwards.end(), belongs_to_client) >= maximum_forwards_per_client)
			pending_forwards.erase(std::find_if(pending_forwards.begin(), pending_forwards.end(), belongs_to_client));

		if (pending_forwards.size() >= maximum_pending_forwards)
		{
			console::debug("[hidden_challenges] pending forward queue is full\n");
			return;
		}

		pending_forwards.push_back({user_id, group, challenge});
	}

	demonware::reward_delivery submit_reward(const std::uint64_t user_id,
		const demonware::reward_game_events::event& event)
	{
		using demonware::reward_delivery;
		if (!accepting_forwards.load() || game::environment::is_zombies()) return reward_delivery::retryable_failure;
		try
		{
			// Serialize acceptance against shutdown, just like hidden completions.
			std::lock_guard lock{pending_forward_mutex};
			if (!accepting_forwards.load()) return reward_delivery::retryable_failure;
			return server_events.push(user_id, event);
		}
		catch (...) { console::warn("[HQ relay] could not queue server event\n"); }
		return reward_delivery::retryable_failure;
	}

	demonware::reward_delivery submit_rewards(const std::vector<demonware::reward_game_events::user_event_batch>& users)
	{
		using demonware::reward_delivery;
		try
		{
			const auto dedicated = game::environment::is_dedicated();
			const auto local_user = dedicated ? 0 : steam::SteamUser()->GetSteamID().bits;
			std::vector<demonware::reward_game_events::event> local;
			std::vector<std::pair<std::uint64_t, demonware::reward_game_events::event>> remote;
			for (const auto& user : users)
			{
				if (user.account_type != "steam") continue;
				for (const auto& event : user.events)
				{
					if (!demonware::achievement_engine::valid_event(event, true)) return reward_delivery::permanent_failure;
					if (!dedicated && user.user_id == local_user) local.push_back(event);
					else remote.emplace_back(user.user_id, event);
				}
			}
			// Serialize the request against shutdown; queue locks never cover store I/O.
			std::lock_guard lock{pending_forward_mutex};
			if (!accepting_forwards.load() || game::environment::is_zombies()) return reward_delivery::retryable_failure;
			return server_events.push_batch(remote, [&] { return demonware::achievement_engine::submit_events(local, true); });
		}
		catch (...) { console::warn("[HQ relay] could not accept reward request\n"); }
		return reward_delivery::retryable_failure;
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			accepting_forwards = true;
			scheduler::loop(process_pending_forwards, scheduler::pipeline::server);
			if (!game::environment::is_zombies()) command::add("hqrelaytest", relay_test_command);

			if (!game::environment::is_dedicated())
			{
				if (!game::environment::is_zombies())
				{
					scheduler::loop(process_client_events, scheduler::pipeline::async, 100ms);
				}
				deploy_server_command_hook.create(game::CG_DeployServerCommandString,
					deploy_server_command_stub);
			}
		}

		void pre_destroy() override
		{
			accepting_forwards = false;
			clear_pending_forwards();
		}
	};
}

REGISTER_COMPONENT(hidden_challenge_relay::component)
