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
	// shared native Orders UI filters Zombies daily/weekly kinds as 8/9. Rotate
	// six daily / three weekly offers through this pool. Only kill-event rules
	// are used: the unfiltered jolts/waves/special rows need additional semantics.
	// Targets and rewards are local policy, not recovered live-service rotations.
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
		{1001, 8, "daily_zm_ch_pest_melee", 15, "Pest Control", "Kill 15 Pests with melee attacks", "(2:2)&&(3:2)"},
		{1014, 8, "daily_zm_ch_sniper_headshots_1", 20, "Deadeye", "Kill 20 zombies with sniper headshots", "(1:5)&&(6:1)"},
		{1010, 8, "daily_zm_ch_kills_traps_1", 50, "Let the Trap Work", "Kill 50 zombies with traps", "(1:11)"},
		{1053, 8, "daily_zm_ch_kills_pistols_upgraded_1", 60, "Sidearm Overdrive", "Kill 60 zombies with upgraded pistols", "(1:6)&&(7:1)"},
		{1011, 8, "daily_zm_ch_kills_bombers_clean", 5, "Bomb Disposal", "Kill 5 Bombers without detonating their bombs", "(2:4)&&(128:128)"},
		{1040, 8, "daily_zm_ch_throwing_knifes_1", 15, "Knife Work", "Kill 15 zombies with throwing knives", "(128:4096)"},
		{1102, 8, "daily_zm_ch_kills_jack_in_box_1", 40, "Surprise Package", "Kill 40 zombies with Jack-in-the-Boxes", "(128:1073741824)"},
		{1004, 8, "daily_zm_ch_wustling_melee_only", 2, "Heavy Handed", "Kill 2 Wustlings using only melee attacks", "(2:3)&&(128:512)"},
		{1037, 8, "daily_zm_ch_bomber_bomb_kills", 15, "Return to Sender", "Kill 15 zombies with Bomber bombs", "(128:256)"},
		{1005, 8, "daily_zm_ch_empty_clip", 3, "Running on Empty", "Kill 3 zombies while out of ammo", "(128:64)"},
		{1047, 8, "daily_zm_ch_kills_ripsaw_1", 25, "Cutting Crew", "Kill 25 zombies with the Ripsaw heavy attack on The Darkest Shore", "(128:134217728)"},
		{1024, 8, "daily_zm_ch_bomber_with_bomb", 1, "Chain Reaction", "Kill a Bomber with a Bomber bomb", "(2:4)&&(128:256)"},
		{1049, 8, "daily_zm_ch_traps_propeller_1", 30, "Propeller Cleanup", "Kill 30 zombies with the Sub Pen trap on The Darkest Shore", "(128:8192)"},
		{1023, 8, "daily_zm_ch_wave_asn_spines_1", 2, "Spine Collector", "Kill 2 Meuchlers with the charged Ripsaw attack on The Darkest Shore", "(2:6)&&(128:4194304)"},
		{1060, 9, "weekly_zm_ch_kills_electrical_1", 200, "High Voltage", "Kill 200 zombies with electrical damage", "(128:32)"},
		{1063, 9, "weekly_zm_ch_kills_benners_1", 10, "Fire Brigade", "Kill 10 Brenners", "(2:5)"},
		{1067, 9, "weekly_zm_ch_ripsaw_kills_no_heavy", 100, "Saw Specialist", "Kill 100 zombies with the Ripsaw without its heavy attack on The Darkest Shore", "(128:8388608)"},
		{1071, 9, "weekly_zm_ch_kills_1", 750, "Shore Leave Denied", "Kill 750 zombies on The Darkest Shore", "(5:3)"},
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
