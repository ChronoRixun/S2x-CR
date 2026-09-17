#pragma once

#include <cstdint>
#include "game/demonware/reward_game_event.hpp"

namespace hidden_challenge_relay
{
	void submit(std::uint64_t user_id, std::uint32_t group, std::uint32_t challenge);

	// A main-quest progression event for a remote player, with the chapter the
	// server attributed it to (hidden_challenges::unknown_chapter for none).
	void submit_progression(std::uint64_t user_id, std::uint32_t kind, std::uint64_t chapter);
	demonware::reward_delivery submit_rewards(const std::vector<demonware::reward_game_events::user_event_batch>& users);
	demonware::reward_delivery submit_reward(std::uint64_t user_id, const demonware::reward_game_events::event& event);
}
