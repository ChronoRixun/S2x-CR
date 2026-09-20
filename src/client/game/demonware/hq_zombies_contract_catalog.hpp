#pragma once

#include "hq_economy.hpp"

namespace demonware::hq_zombies_contract_catalog
{
	struct definition
	{
		unsigned id;
		const char* name;
		const char* title;
		const char* description;
		unsigned target, seconds, token, sku, price;
	};
	// Retail kind-11 names, targets, timers and cost tokens. Prices and the single
	// Zombies supply drop payout are local policy, as with the MP contract catalog.
	inline constexpr definition entries[]{
		{1074, "contract_zm_ch_kills_1", "Zombie Hunter", "Kill 250 zombies", 250, 3000, 0x5000054, 0x0800F031, 100},
		{1075, "contract_zm_ch_kills_2", "Zombie Slayer", "Kill 400 zombies", 400, 3600, 0x5000055, 0x0800F032, 250},
		{1080, "contract_zm_ch_kills_3", "Zombie Exterminator", "Kill 750 zombies", 750, 7200, 0x500005A, 0x0800F033, 450},
	};
	inline hq_economy::achievement achievement(const definition& d)
	{
		hq_economy::achievement a;
		a.name = a.challenge_name = d.name; a.kind = 11;
		a.target = d.target; a.usage_target = d.seconds;
		a.rewards = {{"GRANT_PRODUCT", 6, 1}};
		return a;
	}
}
