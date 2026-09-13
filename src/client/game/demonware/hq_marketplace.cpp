#include <std_include.hpp>
#include "hq_marketplace.hpp"
#include "hq_protocol.hpp"
#include <set>

namespace demonware::hq_marketplace
{
	bool context(byte_buffer* buffer)
	{
		std::string value{};
		return buffer->size() <= 65536 && buffer->read_string(&value) && value == "s2_steam";
	}

	bool parse_skus(byte_buffer* buffer, inventory_request& request, bool* includes_local_sku)
	{
		if (includes_local_sku) *includes_local_sku = false;
		bool show_all{};
		std::uint32_t ids{}, types{}, id{};
		unsigned char type{};
		std::string token{};
		if (!context(buffer) || !buffer->read_uint32(&request.page) || !request.page ||
			!buffer->read_uint32(&request.limit) || !request.limit || request.limit > 100 ||
			!buffer->read_bool(&show_all) || !buffer->read_uint32(&ids) || ids > 100) return false;
		bool selected_id = ids == 0;
		for (std::uint32_t i = 0; i < ids; ++i)
		{
			if (!buffer->read_uint32(&id)) return false;
			selected_id |= id == 1;
		}
		if (!buffer->read_uint32(&types) || types > 256) return false;
		bool selected_type = types == 0;
		for (std::uint32_t i = 0; i < types; ++i)
		{
			if (!buffer->read_ubyte(&type)) return false;
			selected_type |= type == 100;
		}
		if (!buffer->read_string(&token) || token.size() > 64 || !hq_protocol::padding(buffer)) return false;
		if (includes_local_sku) *includes_local_sku = request.page == 1 && selected_id && selected_type && token.empty();
		return true;
	}

	bool parse_inventory(byte_buffer* buffer, inventory_request& request)
	{
		return context(buffer) && buffer->read_uint32(&request.page) && buffer->read_uint32(&request.limit) &&
			request.page && request.limit && request.limit <= 500 && hq_protocol::padding(buffer);
	}

	std::vector<hq_economy::item> inventory_page(const hq_economy::state& data,
		const inventory_request& request, const std::uint64_t now, const bool expired)
	{
		std::vector<hq_economy::item> result{};
		if (!request.page || !request.limit || request.limit > 500) return result;
		const auto offset = (static_cast<std::uint64_t>(request.page) - 1) * request.limit;
		std::uint64_t index{};
		for (const auto& [key, entry] : data.inventory)
		{
			if (!entry.quantity || (entry.expires && entry.expires <= now) != expired) continue;
			if (index++ < offset) continue;
			result.push_back(entry);
			if (result.size() == request.limit) break;
		}
		return result;
	}

	bool parse_put(byte_buffer* buffer, const std::uint64_t local_user, std::vector<hq_economy::item>& items)
	{
		// Provisional: context, count, then the existing bdMarketplaceInventory wire fields.
		std::uint32_t count{};
		if (!context(buffer) || !buffer->read_uint32(&count) || count > 500) return false;
		std::set<std::pair<std::uint32_t, std::uint16_t>> keys{};
		std::vector<hq_economy::item> parsed{};
		for (std::uint32_t i = 0; i < count; ++i)
		{
			std::uint64_t owner{};
			std::string account{}, blob{};
			std::uint32_t xp{}, modified{};
			std::int64_t duration{};
			hq_economy::item entry{};
			if (!buffer->read_uint64(&owner) || owner != local_user || !buffer->read_string(&account) ||
				(account != "steam" && !account.empty()) || !buffer->read_uint32(&entry.guid) || !entry.guid ||
				!buffer->read_uint32(&entry.quantity) || !buffer->read_uint32(&xp) || xp ||
				!buffer->read_blob(&blob) || !blob.empty() || !buffer->read_uint32(&entry.expires) ||
				!buffer->read_int64(&duration) || (duration != -1 && duration != 0) ||
				!buffer->read_uint16(&entry.collision) || !buffer->read_uint32(&modified) ||
				!keys.emplace(entry.guid, entry.collision).second) return false;
			entry.modified = static_cast<std::uint32_t>(time(nullptr));
			parsed.push_back(entry);
		}
		if (!hq_protocol::padding(buffer)) return false;
		items = std::move(parsed);
		return true;
	}

	bool parse_pawn(byte_buffer* buffer, std::string& transaction, std::vector<hq_economy::item>& items)
	{
		// IW7 candidate: context, ClientTx, count, (item ID, resulting quantity, collision).
		std::uint32_t count{};
		if (!context(buffer) || !buffer->read_string(&transaction) || transaction.empty() || transaction.size() > 256 ||
			!buffer->read_uint32(&count) || count > 100) return false;
		std::set<std::pair<std::uint32_t, std::uint16_t>> keys{};
		std::vector<hq_economy::item> parsed{};
		for (std::uint32_t i = 0; i < count; ++i)
		{
			hq_economy::item entry{};
			if (!buffer->read_uint32(&entry.guid) || !entry.guid || !buffer->read_uint32(&entry.quantity) ||
				!buffer->read_uint16(&entry.collision) || !keys.emplace(entry.guid, entry.collision).second) return false;
			parsed.push_back(entry);
		}
		if (!hq_protocol::padding(buffer)) return false;
		items = std::move(parsed);
		return true;
	}

	bool put(const std::vector<hq_economy::item>& items)
	{
		return hq_economy::transact([&](auto& data)
		{
			for (const auto& item : items) data.inventory[{item.guid, item.collision}] = item;
			return true;
		});
	}

	bool pawn(const std::string& transaction, const std::vector<hq_economy::item>& items)
	{
		// Quantity reconciliation only. Never invent a currency payout without pawn values.
		std::string fingerprint{};
		for (const auto& item : items) fingerprint += std::to_string(item.guid) + ":" +
			std::to_string(item.collision) + ":" + std::to_string(item.quantity) + ";";
		if (fingerprint.size() > 1024) return false;
		return hq_economy::transact([&](auto& data)
		{
			const auto key = "pawn:" + transaction;
			const auto prior = data.transactions.find(key);
			if (prior != data.transactions.end()) return prior->second == fingerprint;
			for (const auto& item : items)
			{
				const auto found = data.inventory.find({item.guid, item.collision});
				if (found == data.inventory.end() || item.quantity > found->second.quantity ||
					(found->second.expires && found->second.expires <= time(nullptr))) return false;
				found->second.quantity = item.quantity;
				found->second.modified = static_cast<std::uint32_t>(time(nullptr));
			}
			data.transactions.emplace(key, fingerprint);
			return true;
		});
	}
}
