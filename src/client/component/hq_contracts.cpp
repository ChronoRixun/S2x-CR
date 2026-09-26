#include <std_include.hpp>
#include <sstream>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/ui_scripting/execution.hpp"
#include "ui_scripting.hpp"
#include "game/demonware/hq_contract_catalog.hpp"
#include "game/demonware/hq_zombies_catalog.hpp"
#include "game/demonware/hq_zombies_contract_catalog.hpp"

namespace hq_contracts
{
	// Keep local Orders policy; the Contracts use the loaded retail periodic rows, with their
	// rewards and timers from hq_contract_catalog. Captured AE limits remain authoritative
	// even where the retail table differs.
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
local contractLimits = {}
local rewards = {
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
for _, entry in ipairs(S2xRewards) do
	contractLimits[entry.id] = entry.seconds
	local item = entry.item ~= "" and Engine.GetItemGUIDFromReference(entry.item) or "0x1"
	rewards[entry.id] = entry.currency ~= 0 and { currencyID = entry.currency, currencyAmount = entry.amount }
		or { productID = item, itemID = item }
end
if CONDITIONS.IsZombiesMode() then
	rows, rewards, contractLimits = {}, {}, {}
	-- The inventory tab groups by periodic type; mode-specific styling uses kind 8/9.
	for _, entry in ipairs(S2xZombiesOrders) do
		rows[entry.id] = {tostring(entry.id), entry.kind == 8 and "AEC_DAILY" or "AEC_WEEKLY",
			entry.name, entry.title, entry.description, "", "1", "", tostring(entry.target), "", "0", "", ""}
		rewards[entry.id] = entry.kind == 8 and {currencyID = 6, currencyAmount = 250} or {productID = "0x6", itemID = "0x6"}
	end
	for _, entry in ipairs(S2xZombiesContracts) do
		-- The retail periodic rows use AEC_CONTRACT, although AE uses kind 11.
		rows[entry.id] = {tostring(entry.id), "AEC_CONTRACT", entry.name, entry.title,
			entry.description, "", "1", "", tostring(entry.target), "", tostring(entry.seconds), entry.token}
		contractLimits[entry.id] = entry.seconds
		rewards[entry.id] = {productID = "0x6", itemID = "0x6"}
	end
end
for _, name in ipairs({"AE_GetScheduledChallenges", "AE_GetPlayerActiveChallenges"}) do
	local original = Engine[name]
	Engine[name] = function(...)
		local result = original(...)
		if type(result) == "table" then
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
		and tonumber(key) == 0 then
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
				const auto lua = ui_scripting::get_globals();
				ui_scripting::table rewards;
				int index{};
				for (const auto& definition : demonware::hq_contract_catalog::entries)
				{
					ui_scripting::table entry;
					entry["id"] = definition.id; entry["seconds"] = definition.seconds;
					entry["currency"] = definition.currency; entry["amount"] = definition.amount;
					entry["item"] = definition.item_reference;
					rewards[++index] = entry;
				}
				lua["S2xRewards"] = rewards;
				if (game::environment::is_zombies())
				{
					ui_scripting::table orders;
					index = 0;
					for (const auto& definition : demonware::hq_zombies_catalog::entries)
					{
						ui_scripting::table entry;
						entry["id"] = definition.id; entry["kind"] = definition.kind;
						entry["name"] = definition.name; entry["target"] = definition.target;
						entry["title"] = definition.title; entry["description"] = definition.description;
						orders[++index] = entry;
					}
					lua["S2xZombiesOrders"] = orders;
					ui_scripting::table contracts;
					index = 0;
					for (const auto& definition : demonware::hq_zombies_contract_catalog::entries)
					{
						ui_scripting::table entry;
						entry["id"] = definition.id; entry["name"] = definition.name;
						entry["title"] = definition.title; entry["description"] = definition.description;
						entry["target"] = definition.target; entry["seconds"] = definition.seconds;
						std::ostringstream token; token << "0x" << std::hex << definition.token;
						entry["token"] = token.str();
						contracts[++index] = entry;
					}
					lua["S2xZombiesContracts"] = contracts;
				}
				(void)lua["loadstring"](policy)[0]();
			});
		}
	};
}

REGISTER_COMPONENT(hq_contracts::component)
