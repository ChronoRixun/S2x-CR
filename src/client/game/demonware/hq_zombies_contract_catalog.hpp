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
		const char* predicate;
	};
	// Retail kind-11 names, targets, timers and cost tokens. Prices and the single
	// Zombies supply drop payout are local policy, as with the MP contract catalog.
	// Exclude 1078: its shipped electrical flag does not test wave >= 10.
	// 1081 completes once the native five-species flag is reported (target 1).
	inline constexpr definition entries[]{
		{1074, "contract_zm_ch_kills_1", "Zombie Hunter", "Kill 250 zombies", 250, 3000, 0x5000054, 0x0800F031, 100, ""},
		{1075, "contract_zm_ch_kills_2", "Zombie Slayer", "Kill 400 zombies", 400, 3600, 0x5000055, 0x0800F032, 250, ""},
		{1080, "contract_zm_ch_kills_3", "Zombie Exterminator", "Kill 750 zombies", 750, 7200, 0x500005A, 0x0800F033, 450, ""},
		{1077, "contract_zm_ch_jumping_knifes_1", "Airborne Blades", "Kill 15 zombies with throwing knives while airborne", 15, 900, 0x5000057, 0x0800F034, 150, "(128:4096)&&(128:16777216)"},
		{1081, "contract_zm_ch_unique_kills_fr_1", "Bestiary Bounty", "Kill five different zombie types in one match", 1, 1800, 0x500005B, 0x0800F035, 200, "(128:33554432)"},
		{1082, "contract_zm_ch_beheadings_1", "Ripsaw Reaper", "Behead 50 zombies with the Ripsaw on The Darkest Shore", 50, 3600, 0x500005C, 0x0800F036, 300, "(128:134217728)"},
		{1084, "contract_zm_kills_freefire_1", "Freefire Frenzy", "Kill 175 zombies while Freefire is active", 175, 1800, 0x500005E, 0x0800F037, 350, "(128:2)"},
		{1088, "contract_zm_treasure_1", "Treasure Hunter", "Kill a Treasure Zombie", 1, 10800, 0x5000062, 0x0800F038, 250, "(2:9)"},
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
