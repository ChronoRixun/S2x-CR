#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace demonware::hq_economy
{
	// Recovered from the shipped image: Inventory_ConvertItemToArmoryCredit (binding b39390,
	// 0x11EC10) calls the shared conversion builder 0x2764A0 through 0x276540, whose body is
	// `mov dword ptr [rsp+20h], 6` - the destination currency id. Its neighbour 0x276560
	// (`mov ..., 7`) is the only caller path of Inventory_ConvertItemToSocialScore, so 7 is
	// Social Score, not Armory Credits. Retail agrees: Inventory_GetCachedCollectionItemSku-
	// Price (0x274970) hard-codes currency 6 and the affordability gate (0x274570) compares
	// the price against GetCurrencyBalance(0, 6). 2 is COD Points (owner-confirmed 'CP 200').
	inline constexpr std::uint8_t armory_credits = 6;
	inline constexpr unsigned native_wallet_slots = 13;
	// Balances this client parked in the wrong slots before that was recovered.
	inline constexpr std::uint8_t legacy_credit_currencies[]{7, 2};
	// Local policy; retail payroll amount has not been recovered.
	inline constexpr std::uint32_t payroll_amount = 200;
	// Local policy: Social Score for the two win dailies; retail's 250 was on the 1v1 Pit daily, unreachable solo.
	inline constexpr std::uint32_t social_score_daily = 250;
	bool migrate_payroll(struct state& data);

	struct item
	{
		std::uint32_t guid{};
		std::uint32_t quantity{};
		std::uint16_t collision{};
		std::uint32_t modified{};
		std::uint32_t expires{};
		std::string metadata{};
	};

	inline bool live(const item& entry, const std::uint64_t now)
	{
		return entry.quantity && (!entry.expires || entry.expires > now);
	}

	struct reward
	{
		std::string type{};
		std::uint32_t id{};
		std::uint32_t amount{};
		std::string achievement_name{};
	};

	struct achievement
	{
		std::string name{};
		std::string challenge_name{};
		int kind{1};
		std::uint32_t progress{};
		std::uint32_t target{1};
		std::uint64_t activation{};
		std::uint64_t activation_generation{}; // Store revision that enrolled this activation.
		std::uint64_t completion{};
		std::uint64_t expired_at{};
		std::uint64_t offer_day{};
		std::uint32_t usage_target{};
		std::uint32_t usage{};
		std::string status{"available"};
		std::vector<reward> rewards{};
		std::string claim_transaction{};
		bool master_prestige{}; // Payroll identity used by the last accepted pickup; absent in legacy stores.
	};

	struct state
	{
		std::uint64_t revision{};
		std::map<std::uint8_t, std::uint32_t> currencies{};
		std::map<std::pair<std::uint32_t, std::uint16_t>, item> inventory{};
		std::map<std::string, achievement> achievements{};
		std::map<std::string, std::string> transactions{};
	};

	// Complete persisted receipt key, including its producer prefix.
	inline constexpr std::size_t identifier_limit = 128;
	bool valid_receipt_key(std::string_view key);
	// For receipts that only guard a replay of one request, stored as "sequence:<n>:<request>".
	// Keeps the newest `limit` under `prefix`; legacy receipts without a sequence go first.
	void keep_newest_receipts(state& data, std::string_view prefix, std::size_t limit);

	// Missing storage initializes an empty economy through snapshot()/transact().
	// Unreadable, damaged or locked storage throws store_unavailable while loading:
	// the two achievement fetches use legacy fallback; other callers retain failure handling.
	// Invalid content raises private invalid_store; only on-disk validation failures
	// are translated to store_unavailable. Migration, serialization and allocation
	// errors (including save-time validation) are not store_unavailable.
	class store_unavailable : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	state snapshot();
	// Drop the in-memory copy so the next snapshot()/transact() re-reads the JSON file.
	void invalidate();
	bool transact(const std::function<bool(state&)>& mutation);
	bool grant(state& data, const reward& value);
}
