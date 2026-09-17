#pragma once

#include "hq_economy.hpp"
#include "reward_game_event.hpp"
#include "hq_event_predicate.hpp"
#include <map>

namespace demonware::achievement_engine
{
	// Main-thread native poll consumes once, including when the cache is unavailable.
	bool consume_event_cache_refresh();
	// Re-arm only a refused fetch; the next poll drops it if wallet/context readiness is lost.
	void retry_event_cache_refresh();

	struct cache_update
	{
		bool fetch_user{}; // Rollover can decrease progress; the native push only increases it.
		bool push_counters{}; // Claims also push; the mapper may ignore it, so the fetch still runs.
		std::vector<hq_economy::achievement> counters{};
	};
	// Only the MP client installs a sink. Called after persistence, never from a preview.
	void set_cache_update_sink(std::function<void(cache_update)> sink);
	std::string counter_push(const hq_economy::achievement& counter);

	// Catalog values are copied on the main thread; transports never access game assets.
	// Reconcile persisted daily/weekly offers; deterministic day injection for tests.
	// Purchased live tokens reserve a slot until activation consumes them.
	bool contract_eligible(const hq_economy::state& data, std::string_view name, std::uint64_t now);
	bool advance_contract_time(hq_economy::achievement& entry, std::uint32_t seconds);
	bool advance_contract_time(hq_economy::state& data, std::uint32_t seconds);
	bool reconcile_offers(hq_economy::state& data, std::uint64_t day);
	// Start of the period after the one containing 'day' (next UTC midnight for kinds 1/4,
	// next UTC week boundary for kind 2). Always strictly greater than any time in that day.
	std::uint64_t period_end(int kind, std::uint64_t day);
	void set_event_rules(std::map<std::string, hq_event_predicate::rule> rules);
	void set_catalog(std::vector<hq_economy::achievement> catalog);
	void set_loot_catalog(std::vector<std::uint32_t> items);
	bool valid_event(const reward_game_events::event& event, bool native_payroll = false);
	bool submit_relay_events(std::vector<reward_game_events::event>& events);
	bool submit_events(const std::vector<reward_game_events::event>& events, bool native_payroll = false);
	bool submit_event(const reward_game_events::event& event, bool native_payroll = false);
	std::string dispatch(std::string_view request);
}
