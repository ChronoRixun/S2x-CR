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

	// Project the persisted identity for both pushes and fetches without changing cooldown.
	inline hq_economy::achievement project(hq_economy::achievement entry)
	{
		if (entry.name == "payroll_officer" && entry.master_prestige)
			entry.name = entry.challenge_name = "payroll_officer_masterprestige";
		return entry;
	}

	// The stock kiosk waits four hours since completion. UTC bucket receipts
	// additionally reject delayed retries; crossing a bucket alone is not eligibility.
	inline outcome settle(hq_economy::state& data, const std::int64_t timestamp, const std::uint64_t now, const bool master_prestige = false)
	{
		if (timestamp <= 0) return outcome::rejected;
		const auto seconds = static_cast<std::uint64_t>(timestamp) / 1000000;
		constexpr std::uint64_t period = 4 * 3600;
		if (seconds > now && seconds - now > 300) return outcome::rejected;
		if (seconds / period != now / period) return outcome::stale; // stale batch, acknowledge only
		const auto receipt = "payroll:" + std::to_string(seconds / period);
		if (!hq_economy::valid_receipt_key(receipt)) return outcome::rejected;
		// An already stamped receipt still has a completion record to republish, unless
		// the record was pruned - then there is nothing truthful to tell the kiosk.
		if (data.transactions.contains(receipt))
		{
			const auto entry = data.achievements.find("payroll_officer");
			if (entry == data.achievements.end()) return outcome::stale;
			entry->second.master_prestige = master_prestige;
			return outcome::replayed;
		}
		auto& entry = data.achievements["payroll_officer"];
		// Do not stamp the new bucket during cooldown: a later eligible pickup
		// in that same bucket must still be able to settle (03:59 -> 07:59).
		// A cooldown event never becomes payable merely because its delivery is retried later.
		if (entry.completion && (now < entry.completion || now - entry.completion < period ||
			seconds < entry.completion || seconds - entry.completion < period))
		{
			entry.master_prestige = master_prestige;
			return outcome::replayed;
		}
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
		entry.master_prestige = master_prestige;
		data.transactions[receipt] = std::to_string(timestamp);
		return result;
	}
}
