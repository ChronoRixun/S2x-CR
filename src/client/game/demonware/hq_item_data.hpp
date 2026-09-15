#pragma once

#include "hq_marketplace.hpp"
#include "hq_protocol.hpp"
#include <set>

namespace demonware::hq_item_data
{
	struct update
	{
		std::uint32_t guid{};
		std::uint16_t collision{};
		std::string bytes;
	};

	inline bool parse(byte_buffer* buffer, const std::uint64_t local_user, std::string& transaction, std::vector<update>& updates)
	{
		std::uint32_t count{};
		if (!hq_marketplace::context(buffer) || !buffer->read_string(&transaction) || transaction.empty() ||
			transaction.size() > 24 || !hq_economy::valid_receipt_key("item-data:" + transaction) || !buffer->read_uint32(&count) || !count || count > 30) return false;
		std::vector<update> parsed;
		std::set<std::pair<std::uint32_t, std::uint16_t>> keys;
		for (std::uint32_t i = 0; i < count; ++i)
		{
			std::uint64_t owner{};
			std::string account;
			update entry;
			if (!buffer->read_uint64(&owner) || owner != local_user || !buffer->read_string(&account) ||
				(!account.empty() && account != "steam") || !buffer->read_uint32(&entry.guid) || !entry.guid ||
				!buffer->read_blob(&entry.bytes) || entry.bytes.size() > 64 || !buffer->read_uint16(&entry.collision) ||
				!keys.emplace(entry.guid, entry.collision).second) return false;
			parsed.push_back(std::move(entry));
		}
		if (!hq_protocol::padding(buffer)) return false;
		updates = std::move(parsed);
		return true;
	}

	inline bool apply(const std::string& transaction, const std::vector<update>& updates)
	{
		if (transaction.empty() || transaction.size() > 24 || !hq_economy::valid_receipt_key("item-data:" + transaction) || updates.empty() || updates.size() > 30) return false;
		// Two independent FNV streams keep the receipt bounded; no raw binary in JSON keys.
		std::uint64_t hash = 14695981039346656037ULL, second = 1099511628211ULL;
		const auto mix = [&](const unsigned char byte) { hash = (hash ^ byte) * 1099511628211ULL; second = (second ^ byte) * 0x100000001B3ULL; };
		for (const auto& entry : updates)
		{
			if (!entry.guid || entry.bytes.size() > 64) return false;
			for (unsigned i = 0; i < 4; ++i) mix(static_cast<unsigned char>(entry.guid >> (i * 8)));
			mix(static_cast<unsigned char>(entry.collision)); mix(static_cast<unsigned char>(entry.collision >> 8));
			mix(static_cast<unsigned char>(entry.bytes.size()));
			for (const auto byte : entry.bytes) mix(static_cast<unsigned char>(byte));
		}
		const auto fingerprint = std::to_string(hash) + ":" + std::to_string(second);
		return hq_economy::transact([&](hq_economy::state& data)
		{
			const auto key = "item-data:" + transaction;
			if (const auto it = data.transactions.find(key); it != data.transactions.end()) return it->second == fingerprint;
			for (const auto& entry : updates)
			{
				const auto it = data.inventory.find({entry.guid, entry.collision});
				if (it == data.inventory.end()) return false;
				it->second.metadata = entry.bytes;
			}
			data.transactions[key] = fingerprint;
			return true;
		});
	}

	// bdMarketplaceAuditLogResult::deserialize reads one string, capacity 0x19.
	class audit_result final : public bdTaskResult
	{
	public:
		std::string transaction;
		void serialize(byte_buffer* buffer) override { buffer->write_string(transaction); }
	};
}
