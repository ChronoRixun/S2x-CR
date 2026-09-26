#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include "hq_economy.hpp"

namespace demonware::hq_mail
{
	inline std::atomic_uint32_t native_reads{}, native_redeems{}, rejected_indices{};

	inline bool valid_slot(const int controller, const int slot, const std::uint32_t count)
	{
		return controller >= 0 && controller < 2 && count >= 14 && count <= 4096 &&
			slot >= 0 && static_cast<std::uint32_t>(slot) < count;
	}

	inline std::string empty_slots(std::size_t count)
	{
		// 0x3721A0 clears a message by setting its ID (+0x10) to zero.
		// 0x3726F0 refuses redemption for that ID. Keep the array allocated:
		// 0x372020 walks slots from index 8 without checking the pointer.
		count = std::clamp<std::size_t>(count, 14, 4096);
		constexpr char empty[] = "\x0A\x10\x08\x00\x12\x00\x1A\x00\x22\x00\x2A\x00\x32\x00\x38\x00\x40\x01";
		std::string result;
		result.reserve(count * 18);
		for (std::size_t i = 0; i < count; ++i) result.append(empty, 18);
		return result;
	}
	struct delivery
	{
		std::uint64_t id;
		const char* code;
		const char* title;
		const char* description;
		const char* image;
		std::vector<hq_economy::reward> rewards;
	};
	// Stable IDs/codes are permanent receipts. Never reuse an ID for a different pack.
	inline const std::vector<delivery> deliveries{
		{1, "s2x-mail:welcome-v1", "Welcome to Headquarters", "A welcome pack containing 500 Armory Credits.",
			"s2_armory_credits_icon", {{"GRANT_CURRENCY", hq_economy::armory_credits, 500}}},
		{2, "s2x-mail:anniversary40-v1", "Activision 40th Anniversary Calling Cards",
			"10 calling cards to celebrate Activision's 40th Anniversary!", "voucher_40th_anniversary",
			{{"GRANT_PRODUCT", 0x2400499, 1}, {"GRANT_PRODUCT", 0x240049A, 1}, {"GRANT_PRODUCT", 0x240049B, 1},
				{"GRANT_PRODUCT", 0x240049C, 1}, {"GRANT_PRODUCT", 0x240049D, 1}, {"GRANT_PRODUCT", 0x240049E, 1},
				{"GRANT_PRODUCT", 0x240049F, 1}, {"GRANT_PRODUCT", 0x24004A0, 1}, {"GRANT_PRODUCT", 0x24004A1, 1},
				{"GRANT_PRODUCT", 0x24004A2, 1}}}, // playercard_40th_001..010
		{3, "s2x-mail:community-v1", "Community Event Rewards", "Rewards from the Call of Duty: WWII community events.",
			"voucher_community_helmet",
			{{"GRANT_PRODUCT", 0x663211F, 1}, {"GRANT_PRODUCT", 0x4000C0, 1}, // hat287, grip_collection_community_01
				{"GRANT_PRODUCT", 0x2400270, 1}, {"GRANT_PRODUCT", 0x24003CD, 1}, {"GRANT_PRODUCT", 0x24003F7, 1}, // playercard_incentive_community_01..03
				{"GRANT_PRODUCT", 0x7000062, 1}, {"GRANT_PRODUCT", 0x7000095, 1}, // weaponcharm_mtx5_community, _02
				{"GRANT_PRODUCT", 0x7040023, 1}, {"GRANT_PRODUCT", 0x7040021, 1}, {"GRANT_PRODUCT", 0x7040018, 1}, // camo_mtx9_community, camo_mtx8_03_universal, camo_mtx7_07_universal
				{"GRANT_PRODUCT", 0x1022402, 1}}} // model21_loot3_mp (Cruiser II)
	};
	inline bool pending(const hq_economy::state& state, const delivery& message)
	{
		return !state.transactions.contains("mail:" + std::to_string(message.id));
	}
	inline bool redeem(hq_economy::state& state, std::uint64_t id, std::string_view code)
	{
		const auto found = std::find_if(deliveries.begin(), deliveries.end(), [&](const auto& d) { return d.id == id && code == d.code; });
		if (found == deliveries.end()) return false;
		const auto key = "mail:" + std::to_string(id);
		if (!hq_economy::valid_receipt_key(key)) return false;
		if (const auto receipt = state.transactions.find(key); receipt != state.transactions.end()) return receipt->second == code;
		// Strong rollback guarantee also for callers outside transact().
		auto next = state;
		for (const auto& reward : found->rewards) if (!hq_economy::grant(next, reward)) return false;
		next.transactions.emplace(key, code); state = std::move(next);
		return true;
	}
	inline std::string varint(std::uint64_t value)
	{
		std::string result;
		do { auto byte = static_cast<unsigned char>(value & 127); value >>= 7; result += static_cast<char>(byte | (value ? 128 : 0)); } while (value);
		return result;
	}
	inline std::string blob(unsigned field, const std::string& value)
	{
		return varint((field << 3) | 2) + varint(value.size()) + value;
	}
	inline std::string content(const delivery& message)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		writer.StartObject(); writer.Key("title"); writer.String(message.title);
		writer.Key("description"); writer.String(message.description); writer.EndObject();
		return {buffer.GetString(), buffer.GetSize()};
	}
	inline std::string messages(const hq_economy::state& state, std::size_t count)
	{
		count = std::clamp<std::size_t>(count, 14, 4096);
		std::string result;
		const auto empty = empty_slots(14).substr(0, 18);
		for (std::size_t slot = 0; slot < count; ++slot)
		{
			// Categories 1/2 occupy the first eight slots. Inbox begins at slot 8.
			if (slot < 8 || slot - 8 >= deliveries.size() || !pending(state, deliveries[slot - 8])) { result += empty; continue; }
			const auto& message = deliveries[slot - 8];
			const auto body = content(message);
			if (!message.id || body.size() > 4096 || std::strlen(message.code) > 1024) { result += empty; continue; }
			result += blob(1, varint(8) + varint(message.id) + blob(2, "en-US") + blob(3, body) +
				blob(4, "{}") + blob(5, message.code) + blob(6, "") + varint(56) + varint(0) + varint(64) + varint(1));
		}
		return result;
	}

}
