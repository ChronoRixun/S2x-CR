#pragma once

#include "hq_economy.hpp"
#include <cstddef>

namespace demonware::hq_inventory_cache
{
	// Native20C8B0 projection, consumed by27DD30/27CA20. No virtual members.
	struct record
	{
		std::uint32_t id{}, quantity{}, expires{}, padding{};
		std::int64_t duration{-1};
		std::uint16_t collision{};
		unsigned char tail[6]{};
	};
	static_assert(sizeof(record) == 32 && offsetof(record, duration) == 16 && offsetof(record, collision) == 24);

	inline record project(const hq_economy::item& entry, const std::uint64_t now)
	{
		return {entry.guid, entry.expires && entry.expires <= now ? 0 : entry.quantity,
			entry.expires, 0, -1, entry.collision, {}};
	}
}
