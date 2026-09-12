#pragma once

#include "hq_economy.hpp"
#include "reward_game_event.hpp"

namespace demonware::achievement_engine
{
	// Catalog values are copied on the main thread; transports never access game assets.
	void set_catalog(std::vector<hq_economy::achievement> catalog);
	bool submit_event(const reward_game_events::event& event);
	std::string dispatch(std::string_view request);
}
