#pragma once

#include <cstdint>
#include "game/demonware/reward_game_event.hpp"

namespace hidden_challenge_relay
{
	void submit(std::uint64_t user_id, std::uint32_t group, std::uint32_t challenge);
	void submit_reward(std::uint64_t user_id, const demonware::reward_game_events::event& event);
}
