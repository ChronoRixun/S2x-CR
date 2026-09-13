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
