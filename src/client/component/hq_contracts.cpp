#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/ui_scripting/execution.hpp"
#include "ui_scripting.hpp"

namespace hq_contracts
{
	// Explicit local periodic-table policy for our nine scheduled offers. The retail
	// periodic table is not in the saved asset dumps; do not pretend its display gates,
	// cost tokens or timing agree with a locally generated AE catalog. Keep the adapter
	// scoped to these IDs and this table, including when another mode reuses the VM.
	constexpr auto policy = R"lua(
local lookup = Engine.TableLookup
local rows = {
	[162] = {"162", "AEC_CONTRACT", "contract_4_headshots_tdm", "TDM Headshots Contract", "Get 4 headshots in Team Deathmatch", "", "1", "", "4", "", "1200", "0x50f0001"},
	[561] = {"561", "AEC_CONTRACT", "contract_50_kills_smg", "SMG Kill Contract", "Get 50 SMG kills", "", "1", "", "50", "", "2400", "0x50f0002"},
	[146] = {"146", "AEC_CONTRACT", "contract_55_kills_tdm", "TDM Kill Contract", "Get 55 kills in Team Deathmatch", "", "1", "", "55", "", "3000", "0x50f0003"},
	[3048] = {"3048", "AEC_CONTRACT", "contract_ch_lad", "LAD Machine Gun Contract", "Get 10 headshots with LMGs", "", "1", "", "10", "", "4800", "0x50f0004"},
	[149] = {"149", "AEC_CONTRACT", "contract_45_kills", "Kill Contract", "Get 45 kills", "", "1", "", "45", "", "2400", "0x50f0005"},
	[153] = {"153", "AEC_CONTRACT", "contract_25_kills_dom", "Domination Kill Contract", "Get 25 kills in Domination", "", "1", "", "25", "", "1200", "0x50f0006"},
	[164] = {"164", "AEC_CONTRACT", "contract_9_headshots", "Headshots Contract", "Get 9 headshots", "", "1", "", "9", "", "2400", "0x50f0007"},
	[204] = {"204", "AEC_CONTRACT", "contract_25_kills_lmg", "LMG Kill Contract", "Get 25 LMG kills", "", "1", "", "25", "", "1200", "0x50f0008"},
	[562] = {"562", "AEC_CONTRACT", "contract_50_kills_lmg", "LMG Supply Contract", "Get 50 LMG kills", "", "1", "", "50", "", "3000", "0x50f0009"},
}
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
}
for _, name in ipairs({"AE_GetScheduledChallenges", "AE_GetPlayerActiveChallenges"}) do
	local original = Engine[name]
	Engine[name] = function(...)
		local result = original(...)
		if type(result) == "table" and not CONDITIONS.IsZombiesMode() then
			for _, record in pairs(result) do
				if type(record) == "table" and rewards[tonumber(record.ID)] then
					record.reward = rewards[tonumber(record.ID)]
					record.timeLimit = tonumber(rows[tonumber(record.ID)][11])
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
