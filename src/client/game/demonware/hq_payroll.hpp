#pragma once

#include "hq_economy.hpp"
#include <mutex>
#include <optional>

namespace demonware::hq_payroll
{
	// What one Headquarters Post pickup did to the store. The kiosk cancels its
	// "Unable to get payroll at this time" banner only on an achievementEngine
	// CompletionUpdate event (ui/s2/mail_officer_menu_uc.lua), so every accepted
	// pickup has to publish one - including a replay inside an already settled
	// period. Only the currency grant is once per period.
	enum class outcome
	{
		rejected,  // nothing was recorded; the bdReward task must fail
		stale,     // the batch belongs to another period: acknowledge, publish nothing
		granted,   // new settlement in this period, Armory Credits granted
		replayed,  // this period was already settled: republish the completion
	};

	// The completion push and the one-line record of what it carries.
	struct push
	{
		std::string json{};
		std::string summary{};
	};

	// Transport thread publishes after every accepted pickup. The main thread
	// consumes the native notification; a superseded one is simply dropped.
	inline std::mutex notification_mutex;
	inline std::optional<push> notification;

	// Local policy: one pickup settlement per UTC four-hour period. The event's
	// microsecond timestamp selects the period, so delayed retries cannot earn again.
	inline outcome settle(hq_economy::state& data, const std::int64_t timestamp, const std::uint64_t now)
	{
		if (timestamp <= 0) return outcome::rejected;
		const auto seconds = static_cast<std::uint64_t>(timestamp) / 1000000;
		constexpr std::uint64_t period = 4 * 3600;
		if (seconds > now + 300) return outcome::rejected;
		if (seconds / period != now / period) return outcome::stale; // stale batch, acknowledge only
		const auto receipt = "payroll:" + std::to_string(seconds / period);
		if (!hq_economy::valid_receipt_key(receipt)) return outcome::rejected;
		// An already stamped receipt still has a completion record to republish, unless
		// the record was pruned - then there is nothing truthful to tell the kiosk.
		if (data.transactions.contains(receipt))
			return data.achievements.contains("payroll_officer") ? outcome::replayed : outcome::stale;
		auto& entry = data.achievements["payroll_officer"];
		auto result = outcome::replayed;
		// Respect a legacy manual claim in this period too.
		if (!entry.completion || entry.completion / period < seconds / period)
		{
			if (!hq_economy::grant(data, {"GRANT_CURRENCY", hq_economy::armory_credits, hq_economy::payroll_amount})) return outcome::rejected;
			entry = {};
			entry.name = "payroll_officer"; entry.challenge_name = entry.name;
			entry.kind = 5; entry.target = 1; entry.progress = 1;
			entry.activation = seconds; entry.completion = now; entry.offer_day = now / 86400;
			entry.status = "finished"; entry.claim_transaction = receipt;
			entry.rewards = {{"GRANT_CURRENCY", hq_economy::armory_credits, hq_economy::payroll_amount}};
			result = outcome::granted;
		}
		data.transactions[receipt] = std::to_string(timestamp);
		return result;
	}
}
