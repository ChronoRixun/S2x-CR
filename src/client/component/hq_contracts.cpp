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
	[5] = {"5", "AEC_DAILY", "daily_ch_1v1_wins", "Single Minded", "Win 1 match in the 1v1 Pit", "", "1", "", "1", "", "0", "", ""},
	[45] = {"45", "AEC_DAILY", "daily_ch_assault_kills", "Rifle Adept", "Get 35 rifle kills", "", "1", "", "35", "", "0", "", "", "2x Supply Drops", "s2_supply_drop_icon"},
	[10] = {"10", "AEC_DAILY", "daily_ch_kills", "Daily Kills", "Get 25 kills", "", "1", "", "25", "", "0", "", ""},
	[11] = {"11", "AEC_DAILY", "daily_ch_headshots", "Daily Headshots", "Get 3 headshots", "", "1", "", "3", "", "0", "", ""},
	[6] = {"6", "AEC_DAILY", "daily_ch_commend", "Commend a Soldier", "Give 1 commendation", "", "1", "", "1", "", "0", "", ""},
	[48] = {"48", "AEC_DAILY", "daily_ch_shotgun_kills", "Shotgun Kills", "Get 100 shotgun kills", "", "1", "", "100", "", "0", "", ""},
	[29] = {"29", "AEC_WEEKLY", "weekly_ch_kills", "Turning the Tide", "Get 500 kills", "", "1", "", "500", "", "0", "", ""},
	[30] = {"30", "AEC_WEEKLY", "weekly_ch_wins", "Weekly Wins", "Win 10 matches", "", "1", "", "10", "", "0", "", ""},
	[25] = {"25", "AEC_WEEKLY", "weekly_ch_scorestreak_calls", "Weekly Scorestreaks", "Call in 25 scorestreaks", "", "1", "", "25", "", "0", "", ""},
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
	[5] = { currencyID = 7, currencyAmount = 250 },
	[45] = { productID = "0x1", itemID = "0x1" },
	[10] = { productID = "0x1", itemID = "0x1" },
	[11] = { productID = "0x1", itemID = "0x1" },
	[6] = { currencyID = 7, currencyAmount = 250 },
	[48] = { productID = "0x20000d", itemID = "0x20000d" },
	[29] = { productID = "0x2", itemID = "0x2" },
	[30] = { productID = "0x2", itemID = "0x2" },
	[25] = { productID = "0x2", itemID = "0x2" },
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
