#pragma once

#include <algorithm>
#include <string>

namespace demonware::hq_mail
{
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
}
