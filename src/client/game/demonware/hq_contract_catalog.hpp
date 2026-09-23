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
	inline constexpr definition entries[]{
		{162, "contract_4_headshots_tdm", "TDM Headshots Contract", "Get 4 headshots in Team Deathmatch", 4, 1200, 100, 1, 3000, ""},
		{561, "contract_50_kills_smg", "SMG Kill Contract", "Get 50 SMG kills", 50, 2400, 350, 1, 3000, ""},
		{146, "contract_55_kills_tdm", "TDM Kill Contract", "Get 55 kills in Team Deathmatch", 55, 3000, 450, 0, 1, ""},
		{3048, "contract_ch_lad", "LAD Machine Gun Contract", "Get 10 headshots with LMGs", 10, 4800, 5000, 0, 1, "lad_loot0_mp"},
		{149, "contract_45_kills", "Kill Contract", "Get 45 kills", 45, 2400, 350, 1, 3000, ""},
		{153, "contract_25_kills_dom", "Domination Kill Contract", "Get 25 kills in Domination", 25, 1200, 100, 1, 3000, ""},
		{164, "contract_9_headshots", "Headshots Contract", "Get 9 headshots", 9, 2400, 350, 1, 3000, ""},
		{204, "contract_25_kills_lmg", "LMG Kill Contract", "Get 25 LMG kills", 25, 1200, 100, 1, 3000, ""},
		{562, "contract_50_kills_lmg", "LMG Supply Contract", "Get 50 LMG kills", 50, 3000, 450, 0, 1, ""},
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
