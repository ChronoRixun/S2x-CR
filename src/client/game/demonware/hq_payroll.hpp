#pragma once

#include "hq_economy.hpp"

namespace demonware::hq_payroll
{
	// Local policy: one pickup settlement per UTC four-hour period. The event's
	// microsecond timestamp selects the period, so delayed retries cannot earn again.
	inline bool settle(hq_economy::state& data, const std::int64_t timestamp, const std::uint64_t now)
	{
		if (timestamp <= 0) return false;
		const auto seconds = static_cast<std::uint64_t>(timestamp) / 1000000;
		constexpr std::uint64_t period = 4 * 3600;
		if (seconds > now + 300) return false;
		if (seconds / period != now / period) return true; // stale batch, acknowledge only
		const auto receipt = "payroll:" + std::to_string(seconds / period);
		if (data.transactions.contains(receipt)) return true;
		auto& entry = data.achievements["payroll_officer"];
		// Respect a legacy manual claim in this period too.
		if (!entry.completion || entry.completion / period < seconds / period)
		{
			if (!hq_economy::grant(data, {"GRANT_CURRENCY", 2, 200})) return false;
			entry = {};
			entry.name = "payroll_officer"; entry.challenge_name = entry.name;
			entry.kind = 5; entry.target = 1; entry.progress = 1;
			entry.activation = seconds; entry.completion = now; entry.offer_day = now / 86400;
			entry.status = "finished"; entry.claim_transaction = receipt;
			entry.rewards = {{"GRANT_CURRENCY", 2, 200}};
		}
		data.transactions[receipt] = std::to_string(timestamp);
		return true;
	}
}
