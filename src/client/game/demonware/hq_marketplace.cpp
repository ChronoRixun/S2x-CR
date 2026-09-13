#include <std_include.hpp>
#include "hq_marketplace.hpp"
#include "hq_protocol.hpp"
#include <set>
#include "hq_collection_items.hpp"
#include <charconv>
#include "game/types/demonware.hpp"

using namespace game::demonware;

namespace demonware::hq_marketplace
{
	namespace
	{
		std::mutex sku_mutex;
		std::map<std::uint32_t, unsigned> item_rarities;
	}

	std::vector<std::uint32_t> granted_items(const sku& entry)
	{
		std::vector<std::uint32_t> result;
		for (const auto id : entry.items) if (id) result.push_back(id);
		if (result.empty()) result.push_back(entry.id);
		return result;
	}

	std::vector<sku> catalog()
	{
		std::lock_guard lock{sku_mutex};
		std::vector<sku> result;
		result.reserve(std::size(vendor_skus) + std::size(collection_items));
		// The tagged supply drops come first: only the first 400 entries reach the native
		// SKU cache, and the Quartermaster's FindSKUIDByType scan stops at the first match.
		for (const auto& entry : vendor_skus) result.push_back(entry);
		for (const auto id : collection_items)
		{
			if (std::any_of(std::begin(vendor_skus), std::end(vendor_skus), [id](const auto& entry) { return entry.id == id; })) continue;
			const auto found = item_rarities.find(id);
			const auto rarity = found == item_rarities.end() ? 0 : found->second;
			result.push_back({id, rarity_prices[rarity], 100});
		}
		return result;
	}

	std::optional<sku> find_sku(const std::uint32_t id)
	{
		// The vendor drops are not collection items and carry a fixed price and SKU data.
		for (const auto& entry : vendor_skus) if (entry.id == id) return entry;
		// Collection rendering queries this once per item, often repeatedly.
		if (!std::binary_search(std::begin(collection_items), std::end(collection_items), id)) return std::nullopt;
		std::lock_guard lock{sku_mutex};
		const auto found = item_rarities.find(id);
		return sku{id, rarity_prices[found == item_rarities.end() ? 0 : found->second], 100};
	}

	void set_rarities(const std::map<std::uint32_t, unsigned>& rarities)
	{
		std::lock_guard lock{sku_mutex};
		for (const auto& [id, rarity] : rarities)
			if (rarity < std::size(rarity_prices)) item_rarities[id] = rarity;
	}

	bool parse_skus(byte_buffer* buffer, sku_request& request)
	{
		sku_request parsed;
		bool show_all{};
		std::uint32_t count{}, id{};
		unsigned char type{};
		std::string token;
		if (!context(buffer) || !buffer->read_uint32(&parsed.page) || !parsed.page ||
			!buffer->read_uint32(&parsed.limit) || !parsed.limit || parsed.limit > 100 ||
			!buffer->read_bool(&show_all) || !buffer->read_uint32(&count) || count > 100) return false;
		for (unsigned i = 0; i < count; ++i) { if (!buffer->read_uint32(&id)) return false; parsed.ids.push_back(id); }
		if (!buffer->read_uint32(&count) || count > 256) return false;
		for (unsigned i = 0; i < count; ++i) { if (!buffer->read_ubyte(&type)) return false; parsed.types.push_back(type); }
		if (!buffer->read_string(&token) || token.size() > 64 || !hq_protocol::padding(buffer)) return false;
		// Native278D30 uses page numbers and an empty string. Accept a matching
		// decimal page token for explicit callers; never silently restart paging.
		if (!token.empty())
		{
			unsigned page{};
			const auto p = std::from_chars(token.data(), token.data() + token.size(), page);
			if (p.ec != std::errc{} || p.ptr != token.data() + token.size() || page != parsed.page) return false;
		}
		request = std::move(parsed);
		return true;
	}

	std::vector<sku> sku_page(const sku_request& request)
	{
		std::vector<sku> selected, result;
		if (!request.page || !request.limit || request.limit > 100) return result;
		// Both captured types are supported:100 generic cache,150 collection fetch.
		// Empty type filter selects the canonical100 catalog, without duplicates.
		for (const auto type : {100, 150, 201})
		{
			if (request.types.empty() ? type != 100 : std::find(request.types.begin(), request.types.end(), type) == request.types.end()) continue;
			for (auto entry : catalog())
			{
				if (type == 201 && !*entry.contract) continue;
				if (!request.ids.empty() && std::find(request.ids.begin(), request.ids.end(), entry.id) == request.ids.end()) continue;
				entry.type = static_cast<unsigned char>(type); selected.push_back(entry);
			}
		}
		const auto offset = (std::uint64_t{request.page} - 1) * request.limit;
		for (auto i = offset; i < selected.size() && result.size() < request.limit; ++i) result.push_back(selected[static_cast<std::size_t>(i)]);
		return result;
	}

	unsigned purchase(const std::string& transaction, const std::uint32_t id, const std::uint32_t quantity)
	{
		if (transaction.empty() || transaction.size() > 128 || transaction.find('\0') != std::string::npos || quantity != 1)
			return BD_MARKETPLACE_INVALID_PARAMETER;
		const auto entry = find_sku(id);
		if (!entry) return BD_MARKETPLACE_RESOURCE_NOT_FOUND;
		unsigned error = BD_MARKETPLACE_STORAGE_ERROR;
		const auto ok = hq_economy::transact([&](auto& next)
		{
			const auto key = "purchase:" + transaction;
			const auto fingerprint = std::to_string(id) + ":1";
			if (const auto prior = next.transactions.find(key); prior != next.transactions.end())
			{
				error = BD_MARKETPLACE_RESOURCE_CONFLICT;
				return prior->second == fingerprint;
			}
			// Collection items are owned once; the vendor supply drops are consumables and
			// stay purchasable while one is still in the inventory.
			const auto consumable = entry->consumable;
			const auto owned = next.inventory.find({id, 0});
			if (!consumable && owned != next.inventory.end() && owned->second.quantity)
			{ error = BD_MARKETPLACE_ITEM_MULTIPLE_PURCHASE_ERROR; return false; }
			auto& balance = next.currencies[hq_economy::armory_credits];
			if (balance < entry->price) { error = BD_MARKETPLACE_INSUFFICIENT_FUNDS_ERROR; return false; }
			balance -= entry->price;
			for (const auto item : granted_items(*entry))
				if (!hq_economy::grant(next, {"GRANT_PRODUCT", item, 1})) return false;
			next.transactions.emplace(key, fingerprint);
			return true;
		});
		return ok ? BD_NO_ERROR : error;
	}

	bool context(byte_buffer* buffer)
	{
		std::string value{};
		return buffer->size() <= 65536 && buffer->read_string(&value) && value == "s2_steam";
	}

	bool parse_skus(byte_buffer* buffer, inventory_request& request, bool* includes_local_sku)
	{
		if (includes_local_sku) *includes_local_sku = false;
		sku_request parsed;
		if (!parse_skus(buffer, parsed)) return false;
		request = parsed;
		if (includes_local_sku) *includes_local_sku = !sku_page(parsed).empty();
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
