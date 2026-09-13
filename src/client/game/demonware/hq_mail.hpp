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
		std::vector<hq_economy::reward> rewards;
	};
	// Stable IDs/codes are permanent receipts. Never reuse an ID for a different pack.
	inline const std::vector<delivery> deliveries{
		{1, "s2x-mail:welcome-v1", "Welcome to Headquarters", "A welcome pack containing 500 Armory Credits.",
			{{"GRANT_CURRENCY", hq_economy::armory_credits, 500}}}
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
