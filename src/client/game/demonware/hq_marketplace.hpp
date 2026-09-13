#pragma once

#include "hq_economy.hpp"
#include "byte_buffer.hpp"

namespace demonware::hq_marketplace
{
	struct inventory_request
	{
		std::uint32_t page{};
		std::uint32_t limit{};
	};
	struct sku_request : inventory_request
	{
		std::vector<std::uint32_t> ids;
		std::vector<unsigned char> types;
	};
	struct sku
	{
		std::uint32_t id{}, price{};
		unsigned char type{100};
	};
	// Guessed local AC prices indexed by native item rarity (column29):
	// common, rare, legendary, epic, heroic. Retail prices not recovered.
	inline constexpr std::uint32_t rarity_prices[]{50, 250, 1000, 3000, 5000};
	std::vector<sku> catalog();
	std::optional<sku> find_sku(std::uint32_t id);
	void set_rarities(const std::map<std::uint32_t, unsigned>& rarities);
	bool parse_skus(byte_buffer* buffer, sku_request& request);
	std::vector<sku> sku_page(const sku_request& request);
	// Returns a BD error code; commits debit, item and replay receipt together.
	unsigned purchase(const std::string& transaction, std::uint32_t id, std::uint32_t quantity);
	bool context(byte_buffer* buffer);
	bool parse_skus(byte_buffer* buffer, inventory_request& request, bool* includes_local_sku = nullptr);
	bool parse_inventory(byte_buffer* buffer, inventory_request& request);
	std::vector<hq_economy::item> inventory_page(const hq_economy::state& data,
		const inventory_request& request, std::uint64_t now, bool expired = false);
	bool parse_put(byte_buffer* buffer, std::uint64_t local_user, std::vector<hq_economy::item>& items);
	bool parse_pawn(byte_buffer* buffer, std::string& transaction, std::vector<hq_economy::item>& items);
	bool put(const std::vector<hq_economy::item>& items);
	bool pawn(const std::string& transaction, const std::vector<hq_economy::item>& items);
}
