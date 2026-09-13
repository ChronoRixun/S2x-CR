#pragma once

#include "byte_buffer.hpp"
#include "hq_protocol.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// bdMarketplace task 99.
//
// Evidence (Ghidra S2xFull, image offsets):
//   * 2AF7A0 parses our AE open_supply_drop / proxy-reward reply ("GrantedItems",
//     "GrantedCurrencies", "DetailedInventory") into the per-controller block at
//     87E3B30 + controller * 0x154 (item ids at +0x1C, count at +0xE4; currency
//     pairs at +0xE8, count at +0x150) and then calls 2AF560.
//   * 2AF560 builds a bdByteBuffer over a 0x3FC-byte stack buffer, writes 99 first
//     and then the pending item ids and (currency, amount) pairs, and submits it.
//   * The LUI trace shows Rank.RewardMasterPrestigeSupplyDrops / Rank.ConvertProxyRewards
//     immediately before the first request (build/research/lui-trace-quartermaster.txt).
// So task 99 is the client committing the rewards it was just granted.
//
// PROVISIONAL: the observed request is context + four uint32(1) fields, which does
// not match 2AF560's field order, and the transport plus both completion callbacks
// live in the Arxan-protected region (decompilation fails), so the exact result
// schema is NOT established. Until it is, the reply is a task-level status only:
// registering the task removes the per-frame "missing task '99'" error and its
// unknown-task payload dump, which was the measurable harm (1989 lines in one run).
namespace demonware::hq_proxy_rewards
{
	inline constexpr std::uint8_t task = 99;
	// Maximum fields accepted from the request; 2AF560 can emit 50 items and 13
	// currency pairs, so the ceiling is generous but still bounded.
	inline constexpr std::size_t field_limit = 128;

	struct request
	{
		std::vector<std::uint32_t> fields{};
	};

	inline std::atomic_uint32_t requests{}, rejected{};
	// Answering with a task-level success does not stop the client re-issuing the
	// request (the missing-task fallback already replied that way). The operator can
	// flip this at runtime with `hqtask99 fail` to test the failure branch without a
	// rebuild; the default stays success because it is the observed-safe answer.
	inline std::atomic_bool answer_with_failure{};

	inline std::mutex signature_mutex;
	inline std::string signature;

	inline bool parse(byte_buffer* buffer, request& result)
	{
		request parsed{};
		std::string context{};
		if (buffer->size() > 65536 || !buffer->read_string(&context) || context != "s2_steam") return false;
		while (!hq_protocol::padding(buffer))
		{
			std::uint32_t value{};
			if (parsed.fields.size() >= field_limit || !buffer->read_uint32(&value)) return false;
			parsed.fields.push_back(value);
		}
		result = std::move(parsed);
		return true;
	}

	inline std::string describe(const request& value)
	{
		std::string text{};
		for (const auto field : value.fields) text += (text.empty() ? "" : ",") + std::to_string(field);
		return text;
	}

	// Logs the first request and then only payload changes: the client repeats this
	// task every frame and must never be allowed to flood the console log again.
	inline bool observe(const request& value)
	{
		auto text = describe(value);
		std::lock_guard lock{signature_mutex};
		if (signature == text) return false;
		signature = std::move(text);
		return true;
	}
}
