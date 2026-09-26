#pragma once
#include "hq_economy.hpp"

namespace demonware::hq_contract_catalog
{
	struct definition
	{
		unsigned id;
		const char* name;
		const char* title;
		const char* description;
		unsigned target, seconds, price, currency, amount;
		// A weapon reward is the weapon's Rare loot0 variant: Create-a-Class unlocks a
		// hidden base weapon (StatsTable column 53) only through one of its variants.
		const char* item_reference;
	};
	// The board shows nine consecutive rows and moves one row a day, so a generic contract
	// sits every seventh row and each day offers one or two. Weapon rows are retail's periodic
	// rows, with its string keys as title and description. Retail charged 5000 AC for the LAD;
	// these Rare rewards cost 2500 here because matches pay no Armory Credits yet. 614-619
	// have no retail target and copy or scale one from a retail contract with the same objective.
	inline constexpr definition entries[]{
		{162, "contract_4_headshots_tdm", "TDM Headshots Contract", "Get 4 headshots in Team Deathmatch", 4, 1200, 100, 1, 3000, ""},
		{566, "contract_ch_mas38", "AEC_CONTRACT_SPECIAL_SMG_TITLE", "AEC_CONTRACT_GLOBAL_SMG_KILLS_DESC", 60, 4500, 2500, 0, 1, "mas38_loot0_mp"},
		{567, "contract_ch_mg81", "AEC_CONTRACT_SPECIAL_LMG_TITLE", "AEC_DAILY_LMG_HEADSHOTS_DESC", 25, 4800, 2500, 0, 1, "mg81_loot0_mp"},
		{573, "contract_ch_sten", "AEC_CONTRACT_STEN_TITLE", "AEC_HEADSHOTS_SMG_DESC", 30, 7200, 2500, 0, 1, "sten_loot0_mp"},
		{574, "contract_ch_gewehr", "AEC_CONTRACT_GEWEHR_TITLE", "AEC_WEEKLY_OLD_CAPTAIN_DESC", 200, 7200, 2500, 0, 1, "g43_loot0_mp"},
		{575, "contract_ch_gpmg", "AEC_CONTRACT_GPMG_TITLE", "AEC_DAILY_CROUCH_PRONE_DESC", 75, 7200, 2500, 0, 1, "breda30_loot0_mp"},
		{576, "contract_ch_icepick", "AEC_CONTRACT_ICEPICK_TITLE", "AEC_WEEKLY_MELEE_KILLS_DESC", 50, 7200, 2500, 0, 1, "icepick_loot0_mp"},
		{561, "contract_50_kills_smg", "SMG Kill Contract", "Get 50 SMG kills", 50, 2400, 350, 1, 3000, ""},
		{577, "contract_ch_trenchknife", "AEC_CONTRACT_TRENCHKNIFE_TITLE", "AEC_OBJECTIVE_MELEE_KILL_DESC", 1, 7200, 2500, 0, 1, "trenchknife_loot0_mp"},
		{578, "contract_ch_volk", "AEC_CONTRACT_VOLK_TITLE", "AEC_CONTRACT_HARCORE_KILLS_DESC", 200, 7200, 2500, 0, 1, "volk_loot0_mp"},
		{579, "contract_ch_orso", "AEC_CONTRACT_ORSO_TITLE", "AEC_WEEKLY_HANDLER_DESC", 65, 7200, 2500, 0, 1, "beretta_loot0_mp"},
		{580, "contract_ch_combatknife", "AEC_CONTRACT_COMBATKNIFE_TITLE", "AEC_THROWING_KNIFE_KILLS_DESC", 10, 3600, 2500, 0, 1, "combatknife_loot0_mp"},
		{581, "contract_ch_enfield", "AEC_CONTRACT_ENFIELD_TITLE", "AEC_WEEKLY_PISTOL_KILLS_DESC", 50, 7200, 2500, 0, 1, "enfieldno2_loot0_mp"},
		{582, "contract_ch_revolver", "AEC_CONTRACT_REVOLVER_TITLE", "AEC_SWITCH_WEAPON_KILL_DESC", 20, 7200, 2500, 0, 1, "reich_loot0_mp"},
		{146, "contract_55_kills_tdm", "TDM Kill Contract", "Get 55 kills in Team Deathmatch", 55, 3000, 450, 0, 1, ""},
		{3048, "contract_ch_lad", "LAD Machine Gun Contract", "Get 10 headshots with LMGs", 10, 4800, 2500, 0, 1, "lad_loot0_mp"},
		{614, "contract_ch_baseballbat", "AEC_CONTRACT_BASEBALLBAT_TITLE", "AEC_WEEKLY_MELEE_KILLS_DESC", 25, 3600, 2500, 0, 1, "baseballbat_loot0_mp"},
		{615, "contract_ch_type5", "AEC_CONTRACT_TYPE_5_TITLE", "AEC_WEEKLY_MULTI_KILL_DESC", 10, 3600, 2500, 0, 1, "type5_loot0_mp"},
		{616, "contract_ch_sterling", "AEC_CONTRACT_STERLING_TITLE", "AEC_WEEKLY_HIPFIRE_DESC", 30, 3600, 2500, 0, 1, "sterling_loot0_mp"},
		{617, "contract_ch_arisaka", "AEC_CONTRACT_ARISAKA_TITLE", "AEC_CONTRACT_HEADSHOTS_DESC", 15, 3600, 2500, 0, 1, "arisaka_loot0_mp"},
		{618, "contract_ch_burstrifle", "AEC_CONTRACT_BURSTRIFLE_TITLE", "AEC_WEEKLY_OLD_CAPTAIN_DESC", 60, 3600, 2500, 0, 1, "m1935_loot0_mp"},
		{149, "contract_45_kills", "Kill Contract", "Get 45 kills", 45, 2400, 350, 1, 3000, ""},
		{619, "contract_ch_autom2", "AEC_CONTRACT_AUTOM2_TITLE", "AEC_CONTRACT_ADS_KILLS", 45, 3600, 2500, 0, 1, "m2carbine_loot0_mp"},
		{746, "contract_ch_claymore", "AEC_CONTRACT_CLAYMORE_TITLE", "AEC_WEEKLY_MELEE_KILLS_DESC", 25, 2700, 2500, 0, 1, "sword_loot0_mp"},
		{747, "contract_ch_lever_action", "AEC_CONTRACT_LEVER_ACTION_TITLE", "AEC_DAILY_SNIPER_HEADSHOTS_DESC", 10, 1800, 2500, 0, 1, "leveraction_loot0_mp"},
		{748, "contract_ch_fire_axe", "AEC_CONTRACT_FIRE_AXE_TITLE", "AEC_WEEKLY_MELEE_KILLS_DESC", 15, 1800, 2500, 0, 1, "axe_loot0_mp"},
		{749, "contract_ch_m1919", "AEC_CONTRACT_M1919_TITLE", "AEC_DAILY_LMG_HEADSHOTS_DESC", 10, 4800, 2500, 0, 1, "m1919_loot0_mp"},
		{750, "contract_ch_nambu", "AEC_CONTRACT_NAMBU_TITLE", "AEC_WEEKLY_HIPFIRE_DESC", 30, 3600, 2500, 0, 1, "nambu_loot0_mp"},
		{153, "contract_25_kills_dom", "Domination Kill Contract", "Get 25 kills in Domination", 25, 1200, 100, 1, 3000, ""},
		{751, "contract_ch_blunderbuss", "AEC_CONTRACT_BLUNDERBUSS_TITLE", "AEC_SHOTGUN_FIRESHELL_DESC", 20, 2100, 2500, 0, 1, "blunderbuss_loot0_mp"},
		{752, "contract_ch_ptrs", "AEC_CONTRACT_PTRS_TITLE", "AEC_DAILY_SNIPER_KILLS_DESC", 50, 3600, 2500, 0, 1, "ptrs41_loot0_mp"},
		{847, "contract_ch_avs", "AEC_CONTRACT_AVS_TITLE", "AEC_WEEKLY_OLD_CAPTAIN_DESC", 75, 4500, 2500, 0, 1, "avs36_loot0_mp"},
		{849, "contract_ch_delisle", "AEC_CONTRACT_DELISLE_TITLE", "AEC_DAILY_SNIPER_KILLS_DESC", 50, 3600, 2500, 0, 1, "delisle_loot0_mp"},
		{850, "contract_ch_ribeyrolls", "AEC_CONTRACT_RIBEYROLLS_TITLE", "AEC_HEADSHOTS_SMG_DESC", 15, 5100, 2500, 0, 1, "ribeyrolles_loot0_mp"},
		{851, "contract_ch_3line", "AEC_CONTRACT_3LINE_TITLE", "AEC_DAILY_SNIPER_HEADSHOTS_DESC", 10, 4800, 2500, 0, 1, "mosin_loot0_mp"},
		{164, "contract_9_headshots", "Headshots Contract", "Get 9 headshots", 9, 2400, 350, 1, 3000, ""},
		{879, "contract_ch_vmg", "AEC_CONTRACT_VMG_TITLE", "AEC_DAILY_LMG_HEADSHOTS_DESC", 10, 4800, 2500, 0, 1, "vmg1927_loot0_mp"},
		{880, "contract_ch_tokyo", "AEC_CONTRACT_TOKYO_TITLE", "AEC_DAILY_SMG_KILLS_DESC", 200, 10800, 2500, 0, 1, "tokyo_loot0_mp"},
		{935, "contract_ch_crossbow", "AEC_CONTRACT_CROSSBOW_TITLE", "AEC_THROWING_KNIFE_KILLS_DESC", 5, 3600, 2500, 0, 1, "dp28_loot0_mp"},
		{936, "contract_ch_emp", "AEC_CONTRACT_EMP_TITLE", "AEC_WEEKLY_HIPFIRE_DESC", 30, 3600, 2500, 0, 1, "emp44_loot0_mp"},
		{937, "contract_ch_charlton", "AEC_CONTRACT_CHARLTON_TITLE", "AEC_CONTRACT_ADS_KILLS", 45, 3600, 2500, 0, 1, "charlton_loot0_mp"},
		{939, "contract_ch_sledge", "AEC_CONTRACT_SLEDGE_TITLE", "AEC_CONTRACT_SLEDGE_DESC", 10, 1800, 2500, 0, 1, "hammer_loot0_mp"},
		{204, "contract_25_kills_lmg", "LMG Kill Contract", "Get 25 LMG kills", 25, 1200, 100, 1, 3000, ""},
		{3045, "contract_ch_kg", "AEC_CONTRACT_KG_TITLE", "AEC_DAILY_ASSAULT_KILLS_DESC", 75, 4800, 2500, 0, 1, "kgm21_loot0_mp"},
		{3046, "contract_ch_gdb", "AEC_CONTRACT_GDB_TITLE", "AEC_DAILY_ASSAULT_HEADSHOTS_DESC", 15, 5100, 2500, 0, 1, "grofuss_loot0_mp"},
		{3047, "contract_ch_wimmer", "AEC_CONTRACT_WIMMER_TITLE", "AEC_DAILY_ASSAULT_KILLS_DESC", 150, 7200, 2500, 0, 1, "wimmer_loot0_mp"},
		{3049, "contract_ch_chatellerault", "AEC_CONTRACT_CHATELLERAULT_TITLE", "AEC_DAILY_CROUCH_PRONE_DESC", 50, 6000, 2500, 0, 1, "chatelleroult_loot0_mp"},
		{3050, "contract_ch_austen", "AEC_CONTRACT_AUSTEN_TITLE", "AEC_WEEKLY_HIPFIRE_DESC", 30, 3600, 2500, 0, 1, "austen_loot0_mp"},
		{3051, "contract_ch_bechowiec", "AEC_CONTRACT_BECHOWIEC_TITLE", "AEC_DAILY_SMG_KILLS_DESC", 200, 10800, 2500, 0, 1, "bechowiec_loot0_mp"},
		{562, "contract_50_kills_lmg", "LMG Supply Contract", "Get 50 LMG kills", 50, 3000, 450, 0, 1, ""},
		{3052, "contract_ch_blyskawica", "AEC_CONTRACT_BLYSKAWICA_TITLE", "AEC_WEEKLY_MULTI_KILL_DESC", 10, 3600, 2500, 0, 1, "blyskawica_loot0_mp"},
		{3053, "contract_ch_m2", "AEC_CONTRACT_M2_TITLE", "AEC_WEEKLY_HIPFIRE_DESC", 30, 3600, 2500, 0, 1, "m2hyde_loot0_mp"},
		{3054, "contract_ch_erma", "AEC_CONTRACT_ERMA_TITLE", "AEC_HEADSHOTS_SMG_DESC", 15, 5100, 2500, 0, 1, "erma_loot0_mp"},
		{3055, "contract_ch_m36", "AEC_CONTRACT_M36_TITLE", "AEC_DAILY_SNIPER_HEADSHOTS_DESC", 10, 4800, 2500, 0, 1, "mas36_loot0_mp"},
		{3056, "contract_ch_wz", "AEC_CONTRACT_WZ_TITLE", "AEC_DAILY_SNIPER_KILLS_DESC", 50, 3600, 2500, 0, 1, "wz35_loot0_mp"},
		{3057, "contract_ch_dagger", "AEC_CONTRACT_DAGGER_TITLE", "AEC_WEEKLY_MELEE_KILLS_DESC", 40, 7200, 2500, 0, 1, "dagger_loot0_mp"},
	};
	inline hq_economy::achievement achievement(const definition& d, unsigned weapon = 0)
	{
		hq_economy::achievement a;
		a.name = d.name; a.challenge_name = d.name; a.kind = 4;
		a.target = d.target; a.usage_target = d.seconds;
		a.rewards = {{d.currency ? "GRANT_CURRENCY" : "GRANT_PRODUCT",
			d.currency ? d.currency : (*d.item_reference ? weapon : 1u), d.amount}};
		return a;
	}
}
