#pragma once
#include "game/demonware/reward/game_event.hpp"
namespace cr_progression
{
 bool process(const demonware::reward_game_events::event& event, std::uint64_t user);
}
