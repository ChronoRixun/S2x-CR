#pragma once

#include "hq_economy.hpp"
#include "reward_game_event.hpp"

namespace demonware::achievement_engine
{
	// Catalog values are copied on the main thread; transports never access game assets.
	// Reconcile persisted daily/weekly offers; deterministic day injection for tests.
	bool reconcile_offers(hq_economy::state& data, std::uint64_t day);
	// Start of the period after the one containing 'day' (next UTC midnight for kinds 1/4,
	// next UTC week boundary for kind 2). Always strictly greater than any time in that day.
	std::uint64_t period_end(int kind, std::uint64_t day);
	void set_catalog(std::vector<hq_economy::achievement> catalog);
	void set_loot_catalog(std::vector<std::uint32_t> items);
	bool submit_event(const reward_game_events::event& event, bool native_payroll = false);
	std::string dispatch(std::string_view request);
}
