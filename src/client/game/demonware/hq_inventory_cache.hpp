#pragma once

#include "hq_economy.hpp"
#include <cstddef>
#include <limits>

namespace demonware::hq_inventory_cache
{
	// Native20C8B0 projection, consumed by27DD30/27CA20. No virtual members.
	struct record
	{
		std::uint32_t id{}, quantity{}, expires{}, padding{};
		std::int64_t duration{std::numeric_limits<std::int64_t>::max()};
		std::uint16_t collision{};
		unsigned char tail[6]{};
	};
	static_assert(sizeof(record) == 32 && offsetof(record, duration) == 16 && offsetof(record, collision) == 24);

	inline record project(const hq_economy::item& entry, const std::uint64_t now)
	{
		return {entry.guid, entry.expires && entry.expires <= now ? 0 : entry.quantity,
			// 27A260 considers expiry 0 / duration -1 expired, even with quantity > 0.
			// Permanent native items require both sentinels (also used by 27DD30).
			entry.expires ? entry.expires : UINT32_MAX, 0,
			std::numeric_limits<std::int64_t>::max(), entry.collision, {}};
	}
	// The purchase callback projects this same bdMarketplaceInventory shape via 20C8B0.
	// Keep task 165 and immediate purchase insertion on the same expiry convention.
	template <typename Result>
	void fill_result(Result& result, const hq_economy::item& entry, const std::uint64_t user,
		const std::uint64_t now)
	{
		const auto value = project(entry, now);
		result.m_playerId = user; result.unk = "steam";
		result.m_itemId = value.id; result.m_itemQuantity = value.quantity;
		result.m_itemXp = 0; result.m_itemData = entry.metadata;
		result.m_expireDateTime = value.expires; result.m_expiryDuration = value.duration;
		result.m_collisionField = value.collision; result.m_modDateTime = entry.modified;
	}
}
