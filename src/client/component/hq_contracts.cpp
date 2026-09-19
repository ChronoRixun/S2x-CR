#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/ui_scripting/execution.hpp"
#include "ui_scripting.hpp"

namespace hq_contracts
{
	// Keep local Orders policy; the nine Contracts use the loaded retail periodic rows.
	// Captured AE limits remain authoritative even where the retail table differs.
	constexpr auto policy = R"lua(
local lookup = Engine.TableLookup
local rows = {
	[10] = {"10", "AEC_DAILY", "daily_ch_kills", "Daily Kills", "Get 25 kills", "", "1", "", "25", "", "0", "", ""},
	[11] = {"11", "AEC_DAILY", "daily_ch_headshots", "Daily Headshots", "Get 3 headshots", "", "1", "", "3", "", "0", "", ""},
	[45] = {"45", "AEC_DAILY", "daily_ch_assault_kills", "Rifle Adept", "Get 35 rifle kills", "", "1", "", "35", "", "0", "", ""},
	[46] = {"46", "AEC_DAILY", "daily_ch_smg_kills", "Run and Gun", "Get 30 SMG kills", "", "1", "", "30", "", "0", "", ""},
	[47] = {"47", "AEC_DAILY", "daily_ch_lmg_kills", "Suppressive Fire", "Get 25 LMG kills", "", "1", "", "25", "", "0", "", ""},
	[48] = {"48", "AEC_DAILY", "daily_ch_shotgun_kills", "Close Quarters", "Get 25 shotgun kills", "", "1", "", "25", "", "0", "", ""},
	[209] = {"209", "AEC_DAILY", "daily_ch_sniper_kills", "Marksman", "Get 15 sniper kills", "", "1", "", "15", "", "0", "", ""},
	[49] = {"49", "AEC_DAILY", "daily_ch_pistol_kills", "Sidearm Specialist", "Get 10 pistol kills", "", "1", "", "10", "", "0", "", ""},
	[16] = {"16", "AEC_DAILY", "daily_ch_shovel_kills", "Trench Warfare", "Get 5 melee kills", "", "1", "", "5", "", "0", "", ""},
	[210] = {"210", "AEC_DAILY", "daily_ch_assault_headshots", "Precision Rifle", "Get 5 rifle headshots", "", "1", "", "5", "", "0", "", ""},
	[211] = {"211", "AEC_DAILY", "daily_ch_sniper_headshots", "Between the Eyes", "Get 3 sniper headshots", "", "1", "", "3", "", "0", "", ""},
	[212] = {"212", "AEC_DAILY", "daily_ch_lmg_headshots", "Heavy Precision", "Get 5 LMG headshots", "", "1", "", "5", "", "0", "", ""},
	[12] = {"12", "AEC_DAILY", "daily_ch_dom_wins", "Domination Victor", "Win 1 Domination match", "", "1", "", "1", "", "0", "", ""},
	[13] = {"13", "AEC_DAILY", "daily_ch_tdm_wins", "Team Player", "Win 1 Team Deathmatch", "", "1", "", "1", "", "0", "", ""},
	[14] = {"14", "AEC_DAILY", "daily_ch_killstreak", "Air Support", "Call in 3 scorestreaks", "", "1", "", "3", "", "0", "", ""},
	[15] = {"15", "AEC_DAILY", "daily_ch_dom_caps", "Flag Runner", "Capture 5 Domination points", "", "1", "", "5", "", "0", "", ""},
	[269] = {"269", "AEC_DAILY", "daily_ch_assists", "Team Effort", "Get 10 assists", "", "1", "", "10", "", "0", "", ""},
	[213] = {"213", "AEC_DAILY", "daily_ch_destroy_scorestreaks", "Anti-Air", "Destroy 2 enemy scorestreaks", "", "1", "", "2", "", "0", "", ""},
	[37] = {"37", "AEC_DAILY", "daily_ch_equipment_kills", "Lethal Throwback", "Get 5 equipment kills", "", "1", "", "5", "", "0", "", ""},
	[221] = {"221", "AEC_DAILY", "daily_ch_ffa_killer", "Lone Wolf", "Get 15 kills in Free-for-All", "", "1", "", "15", "", "0", "", ""},
	[29] = {"29", "AEC_WEEKLY", "weekly_ch_kills", "Turning the Tide", "Get 500 kills", "", "1", "", "500", "", "0", "", ""},
	[30] = {"30", "AEC_WEEKLY", "weekly_ch_wins", "Weekly Wins", "Win 10 matches", "", "1", "", "10", "", "0", "", ""},
	[25] = {"25", "AEC_WEEKLY", "weekly_ch_scorestreak_calls", "Weekly Scorestreaks", "Call in 25 scorestreaks", "", "1", "", "25", "", "0", "", ""},
	[247] = {"247", "AEC_WEEKLY", "weekly_ch_infantry_kills", "Infantry Division", "Get 100 kills using Infantry", "", "1", "", "100", "", "0", "", ""},
	[248] = {"248", "AEC_WEEKLY", "weekly_ch_airborne_kills", "Airborne Division", "Get 100 kills using Airborne", "", "1", "", "100", "", "0", "", ""},
	[249] = {"249", "AEC_WEEKLY", "weekly_ch_armored_kills", "Armored Division", "Get 100 kills using Armored", "", "1", "", "100", "", "0", "", ""},
	[250] = {"250", "AEC_WEEKLY", "weekly_ch_mountain_kills", "Mountain Division", "Get 100 kills using Mountain", "", "1", "", "100", "", "0", "", ""},
	[251] = {"251", "AEC_WEEKLY", "weekly_ch_expeditionary_kills", "Expeditionary Division", "Get 100 kills using Expeditionary", "", "1", "", "100", "", "0", "", ""},
	[265] = {"265", "AEC_WEEKLY", "weekly_ch_long_range_kills", "Long Range Specialist", "Get 50 long range kills", "", "1", "", "50", "", "0", "", ""},
	[266] = {"266", "AEC_WEEKLY", "weekly_ch_explosive_kills", "Demolitions Expert", "Get 50 explosive kills", "", "1", "", "50", "", "0", "", ""},
}
local contractLimits = { [162] = 1200, [561] = 2400, [146] = 3000, [3048] = 4800,
	[149] = 2400, [153] = 1200, [164] = 2400, [204] = 1200, [562] = 3000 }
local rewards = {
	[162] = { currencyID = 1, currencyAmount = 3000 },
	[561] = { currencyID = 1, currencyAmount = 3000 },
	[146] = { productID = "0x1", itemID = "0x1" },
	[3048] = { productID = Engine.GetItemGUIDFromReference("lad_mp"), itemID = Engine.GetItemGUIDFromReference("lad_mp") },
	[149] = { currencyID = 1, currencyAmount = 3000 },
	[153] = { currencyID = 1, currencyAmount = 3000 },
	[164] = { currencyID = 1, currencyAmount = 3000 },
	[204] = { currencyID = 1, currencyAmount = 3000 },
	[562] = { productID = "0x1", itemID = "0x1" },
	[10] = { productID = "0x1", itemID = "0x1" },
	[11] = { productID = "0x1", itemID = "0x1" },
	[45] = { productID = "0x1", itemID = "0x1" },
	[46] = { productID = "0x1", itemID = "0x1" },
	[47] = { productID = "0x1", itemID = "0x1" },
	[48] = { productID = "0x1", itemID = "0x1" },
	[209] = { productID = "0x1", itemID = "0x1" },
	[49] = { productID = "0x1", itemID = "0x1" },
	[16] = { currencyID = 1, currencyAmount = 500 },
	[210] = { productID = "0x1", itemID = "0x1" },
	[211] = { productID = "0x1", itemID = "0x1" },
	[212] = { productID = "0x1", itemID = "0x1" },
	[12] = { productID = "0x1", itemID = "0x1" },
	[13] = { productID = "0x1", itemID = "0x1" },
	[14] = { currencyID = 1, currencyAmount = 300 },
	[15] = { currencyID = 1, currencyAmount = 250 },
	[269] = { currencyID = 1, currencyAmount = 250 },
	[213] = { currencyID = 1, currencyAmount = 300 },
	[37] = { productID = "0x1", itemID = "0x1" },
	[221] = { productID = "0x1", itemID = "0x1" },
	[29] = { productID = "0x2", itemID = "0x2" },
	[30] = { productID = "0x2", itemID = "0x2" },
	[25] = { productID = "0x2", itemID = "0x2" },
	[247] = { productID = "0x2", itemID = "0x2" },
	[248] = { productID = "0x2", itemID = "0x2" },
	[249] = { productID = "0x2", itemID = "0x2" },
	[250] = { productID = "0x2", itemID = "0x2" },
	[251] = { productID = "0x2", itemID = "0x2" },
	[265] = { productID = "0x2", itemID = "0x2" },
	[266] = { productID = "0x2", itemID = "0x2" },
}
for _, name in ipairs({"AE_GetScheduledChallenges", "AE_GetPlayerActiveChallenges"}) do
	local original = Engine[name]
	Engine[name] = function(...)
		local result = original(...)
		if type(result) == "table" and not CONDITIONS.IsZombiesMode() then
			for _, record in pairs(result) do
				if type(record) == "table" and rewards[tonumber(record.ID)] then
					record.reward = rewards[tonumber(record.ID)]
					local id = tonumber(record.ID)
					record.timeLimit = contractLimits[id] or (rows[id] and tonumber(rows[id][11]))
				end
			end
		end
		return result
	end
end

Engine.TableLookup = function(file, key, value, column, ...)
	if type(file) == "string" and string.lower(file) == "mp/periodicchallengetable.csv"
		and tonumber(key) == 0 and not (CONDITIONS and CONDITIONS.IsZombiesMode()) then
		local row = rows[tonumber(value)]
		local index = tonumber(column)
		if row and index and index >= 0 and index < 20 and index == math.floor(index) then
			-- Local offers are not gated by live-service cohorts, unlock/lock items,
			-- randomizers or purchase-conversion overrides (columns 12..19).
			return row[index + 1] or ""
		end
	end
	return lookup(file, key, value, column, ...)
end
)lua";

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated()) return;
			ui_scripting::on_start([]
			{
				if (game::environment::is_zombies()) return;
				const auto lua = ui_scripting::get_globals();
				(void)lua["loadstring"](policy)[0]();
			});
		}
	};
}

REGISTER_COMPONENT(hq_contracts::component)
