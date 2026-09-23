#pragma once

#include "hq_protocol.hpp"
#include "hq_marketplace.hpp"
#include "achievement_engine.hpp"
#include "game/types/demonware.hpp"
#include <utils/cryptography.hpp>
#include <utils/string.hpp>

namespace demonware::hq_vendor
{
	inline std::atomic_uint32_t requests{}, replies{}, rejected{};

	inline std::int64_t now_ms()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	// steady_clock ms at which the last 242 reply was queued for the client (0 = none).
	// The native callbacks report their distance from it: a healthy round trip is a few
	// ms; ~30 s means the reply sat in the socket queue while the game thread was stalled.
	inline std::atomic<std::int64_t> last_reply_ms{};

	// A conversion rule id is MD5(name) printed as a UUID whose first three
	// groups are byte-swapped (278180).
	inline std::string rule_id(const std::string& name)
	{
		unsigned char digest[16]{};
		hash_state state;
		md5_init(&state);
		md5_process(&state, reinterpret_cast<const unsigned char*>(name.data()), static_cast<unsigned long>(name.size()));
		md5_done(&state, digest);
		std::reverse(digest, digest + 4);
		std::swap(digest[4], digest[5]);
		std::swap(digest[6], digest[7]);
		std::string id;
		for (auto i = 0; i < 16; ++i)
		{
			id += utils::string::va("%02x", digest[i]);
			if (i == 3 || i == 5 || i == 7 || i == 9) id += '-';
		}
		return id;
	}

	// Task 242 is applyConversionRule. Native response reader A4C850 expects
	// transaction string, uint64, rule object, then repeated currency/item records.
	inline bool reply_body(const std::string& request, std::string& response, std::uint32_t& error)
	{
		error = game::demonware::BD_PARAM_PARSE_ERROR;
		std::size_t at{};
		std::string fields[3];
		for (unsigned i = 0; i < 3; ++i)
		{
			if (at + 2 > request.size() || static_cast<unsigned char>(request[at++]) != (i + 1) * 8 + 2) return false;
			const auto size = static_cast<unsigned char>(request[at++]);
			if (size > 64 || size > request.size() - at) return false;
			fields[i] = request.substr(at, size);
			at += size;
			if (fields[i].find('\0') != std::string::npos) return false;
		}
		const auto count = request.size() == at + 2 && request[at] == '\x20' ? static_cast<unsigned char>(request[at + 1]) : 0u;
		if (fields[0] != "s2_steam" || fields[2].empty() || fields[2].size() > 24 || !count || count > 127) return false;
		if (fields[1] != "3cf6ce39-7313-4bd0-1fcf-c8ba7b0eecd6")
		{
			// The duplicate pump (276C20) sends "Pawnable_Uniform_<GUID>" to pawn `count` spare
			// copies of an "Any"-division uniform (652250 returns 0). Pay them at the supply-drop
			// duplicate rate. 0x1F6A is the one error its queue (275570) drops instead of retrying:
			// it answers a replay and keeps a uniform with no known pawn value.
			if (!hq_economy::transact([&](auto& data)
			{
				const auto now = static_cast<std::uint32_t>(time(nullptr));
				for (auto& [key, entry] : data.inventory)
				{
					if (key.second || (key.first & 0x7F00000) != 0x6000000 ||
						rule_id(utils::string::va("Pawnable_Uniform_%X", key.first)) != fields[1]) continue;
					const auto credit = achievement_engine::duplicate_credit(key.first);
					if (!credit || !hq_economy::live(entry, now) || entry.quantity <= count)
					{
						error = game::demonware::BD_MARKETPLACE_INSUFFICIENT_ITEM_QUANTITY;
						return false;
					}
					entry.quantity -= count;
					entry.modified = now;
					return hq_economy::grant(data, {"GRANT_CURRENCY", hq_economy::armory_credits, count * credit});
				}
				return false;
			})) return false;
		}
		response.clear();
		// The startup rule grants nothing; a pawn reaches the native cache through the store sync.
		// Scalar semantics are provisional; field types/limits are native-confirmed.
		response += '\x0A'; response += static_cast<char>(fields[2].size()); response += fields[2];
		response.append("\x10\x00", 2);
		std::string rule;
		rule += '\x0A'; rule += static_cast<char>(fields[0].size()); rule += fields[0];
		rule.append("\x12\x00", 2); // unknown display/name string
		rule += '\x1A'; rule += static_cast<char>(fields[1].size()); rule += fields[1];
		rule += '\x20'; rule += static_cast<char>(count);
		response += '\x1A'; response += static_cast<char>(rule.size()); response += rule;
		// Fields 4..6 are absent, i.e. zero repeated records. Native loops use counts.

		return true;
	}

	// Read-side chain A4A5A0 -> A4A510 -> A4A2C0, native stride 0x370.
	// SKU and product ID equal the purchasable item GUID (local policy).
	class catalog_result final : public bdTaskResult
	{
	public:
		hq_marketplace::sku entry;
		void serialize(byte_buffer* buffer) override
		{
			buffer->write_uint32(entry.id); // +20 SKU ID
			buffer->write_uint32(entry.id); // +24 product ID
			buffer->write_ubyte(1); // +28
			buffer->write_blob(std::string{entry.data} + '\0'); // +29 bounded SKU data (64 bytes)
			buffer->write_ubyte(1); // +6A
			buffer->write_uint32(0); // +6C
			buffer->write_uint32(0); // +70 sale end
			buffer->write_uint32(0); // +74
			buffer->write_ubyte(0); // +78
			buffer->write_blob(std::string{entry.promotional_text} + '\0'); // +80 promotional text (135 bytes)
			buffer->write_uint32(0); // +10C
			buffer->write_uint16(0); // +110
			buffer->write_uint32(0); // +114
			buffer->write_ubyte(0); // +6B
			buffer->write_uint32(1); // +118 price count, fixed native capacity 10
			buffer->write_ubyte(entry.currency); // price +20 currency ID
			buffer->write_uint32(entry.price); // price +24 absolute price
			buffer->write_ubyte(entry.type); // +350 SKU type
			buffer->write_uint32(1); // +358 max quantity
			buffer->write_bool(false); // +35C sold out
		}
	};

	class result final : public bdTaskResult
	{
	public:
		std::string body;
		void serialize(byte_buffer* buffer) override
		{
			buffer->write_struct(body.data(), static_cast<int>(body.size()));
		}
	};
}
