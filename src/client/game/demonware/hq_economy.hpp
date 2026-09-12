#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace demonware::hq_economy
{
	struct item
	{
		std::uint32_t guid{};
		std::uint32_t quantity{};
		std::uint16_t collision{};
		std::uint32_t modified{};
		std::uint32_t expires{};
	};

	struct reward
	{
		std::string type{};
		std::uint32_t id{};
		std::uint32_t amount{};
		std::string achievement_name{};
	};

	struct achievement
	{
		std::string name{};
		std::string challenge_name{};
		int kind{1};
		std::uint32_t progress{};
		std::uint32_t target{1};
		std::uint64_t activation{};
		std::uint64_t completion{};
		std::uint64_t offer_day{};
		std::uint32_t usage_target{};
		std::uint32_t usage{};
		std::string status{"available"};
		std::vector<reward> rewards{};
		std::string claim_transaction{};
	};

	struct state
	{
		std::uint64_t revision{};
		std::map<std::uint8_t, std::uint32_t> currencies{};
		std::map<std::pair<std::uint32_t, std::uint16_t>, item> inventory{};
		std::map<std::string, achievement> achievements{};
		std::map<std::string, std::string> transactions{};
	};

	state snapshot();
	bool transact(const std::function<bool(state&)>& mutation);
	bool grant(state& data, const reward& value);
}
