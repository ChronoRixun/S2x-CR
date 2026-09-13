#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "game/game.hpp"
#include "game/ui_scripting/execution.hpp"
#include "ui_scripting.hpp"

namespace hq_contracts
{
	// Explicit local periodic-table policy for our three scheduled offers. The retail
	// periodic table is not in the saved asset dumps; do not pretend its display gates,
	// cost tokens or timing agree with a locally generated AE catalog. Keep the adapter
	// scoped to these IDs and this table, including when another mode reuses the VM.
	constexpr auto policy = R"lua(
local lookup = Engine.TableLookup
local rows = {
	[33] = {"33", "AEC_CONTRACT", "contract_mp_1", "Contract 1", "Complete a match", "", "1", "", "1", "", "3600", "0x5000001"},
	[34] = {"34", "AEC_CONTRACT", "contract_mp_2", "Contract 2", "Get a headshot", "", "1", "", "1", "", "3600", "0x5000002"},
	[35] = {"35", "AEC_CONTRACT", "contract_mp_3", "Contract 3", "Get a multi-kill", "", "1", "", "1", "", "3600", "0x5000003"}
}
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
