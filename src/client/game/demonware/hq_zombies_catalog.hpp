#pragma once

#include "hq_economy.hpp"
#include "hq_event_predicate.hpp"

namespace demonware::hq_zombies_catalog
{
	struct definition
	{
		int id, kind;
		const char* name;
		unsigned target;
		const char* title;
		const char* description;
		const char* predicate;
	};

	// Retail identities and kill predicates from dw/dwGameChallenges.csv. The
	// shared native Orders UI filters Zombies daily/weekly kinds as 8/9. Targets
	// and rewards are local policy, not recovered live-service rotations.
	inline constexpr definition entries[] = {
		{1025, 8, "daily_zm_ch_headshots_1", 25, "Dead on Target", "Kill 25 zombies with headshots", "(6:1)"},
		{1028, 8, "daily_zm_ch_shotguns_1", 50, "Close Quarters", "Kill 50 zombies with shotguns", "(1:4)"},
		{1031, 8, "daily_zm_ch_pistols_1", 50, "Sidearm Specialist", "Kill 50 zombies with pistols", "(1:6)"},
		{1034, 8, "daily_zm_ch_lmgs_1", 75, "Suppressive Fire", "Kill 75 zombies with LMGs", "(1:3)"},
		{1016, 8, "daily_zm_ch_kills_upgraded_1", 100, "Improved Firepower", "Kill 100 zombies with upgraded weapons", "(7:1)"},
		{1039, 8, "daily_zm_ch_explosives_1", 40, "Explosive Results", "Kill 40 zombies with explosives", "(3:3)"},
		{1072, 9, "weekly_zm_ch_headshots_1", 200, "Precision Week", "Kill 200 zombies with headshots", "(6:1)"},
		{1068, 9, "weekly_zm_ch_kills_equipment", 150, "Well Equipped", "Kill 150 zombies with equipment", "(1:8)"},
		{1105, 9, "weekly_zm_ch_traps_1", 150, "Death Trap", "Kill 150 zombies with traps", "(1:11)"},
	};

	inline hq_economy::achievement achievement(const definition& row)
	{
		hq_economy::achievement result;
		result.name = result.challenge_name = row.name;
		result.kind = row.kind;
		result.target = row.target;
		result.rewards = row.kind == 8
			? std::vector<hq_economy::reward>{{"GRANT_CURRENCY", hq_economy::armory_credits, 250}}
			: std::vector<hq_economy::reward>{{"GRANT_PRODUCT", 6, 1}};
		return result;
	}
}
