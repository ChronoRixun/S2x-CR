#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "scheduler.hpp"
#include "console/console.hpp"

#include "game/game.hpp"

#include <utils/hook.hpp>

namespace unlock_items
{
	namespace
	{
		const game::dvar_t* cg_unlock_all_items{};
		const game::dvar_t* cg_unlock_debug{};

		utils::hook::detour live_storage_is_item_unlocked_from_table_hook;
		utils::hook::detour live_storage_is_item_unlocked_from_table_local_client_hook;

		// Diagnostics for cg_unlock_debug: how often the unlock hooks run and
		// in which client state, so a locked item can be traced to its cause.
		std::atomic_uint32_t table_calls{};
		std::atomic_uint32_t local_client_calls{};
		std::atomic_uint32_t overridden_calls{};
		std::atomic<const char*> last_table_name{};

		bool unlock_all_enabled()
		{
			return cg_unlock_all_items && cg_unlock_all_items->current.enabled;
		}

		bool debug_enabled()
		{
			return cg_unlock_debug && cg_unlock_debug->current.enabled;
		}

		void record_call(std::atomic_uint32_t& counter, const bool overridden, const game::StringTable* table)
		{
			if (!debug_enabled())
			{
				return;
			}

			++counter;

			if (overridden)
			{
				++overridden_calls;
			}

			if (table && table->name)
			{
				last_table_name = table->name;
			}
		}

		void report_calls()
		{
			if (!debug_enabled())
			{
				// Drain what an earlier enabled window left behind, so the next report
				// only covers calls made while the diagnostics were on.
				table_calls.store(0);
				local_client_calls.store(0);
				overridden_calls.store(0);
				last_table_name.store(nullptr);
				return;
			}

			const auto table = table_calls.exchange(0);
			const auto local = local_client_calls.exchange(0);
			const auto overridden = overridden_calls.exchange(0);
			if (!table && !local)
			{
				return;
			}

			const auto controller = game::CL_ControllerIndexFromClientNum(0);
			const auto has_stats = controller >= 0 && game::LiveStorage_DoWeHaveStats(controller);
			const auto* table_name = last_table_name.load();

			console::info("[unlock_items] last 2s: %u table calls, %u local-client calls, %u overridden | "
				"cg_unlockall_items=%d local_play=%d has_stats=%d last_table=%s\n",
				table, local, overridden, unlock_all_enabled(), game::is_local_play(), has_stats,
				table_name ? table_name : "-");
		}

		bool is_normal_unlock(const game::StringTable* unlock_table, const int row)
		{
			if (!unlock_table || !unlock_table->values || row < 0 || row >= unlock_table->rowCount ||
				unlock_table->columnCount <= 1)
			{
				return false;
			}

			const auto* unlock_type = unlock_table->values[row * unlock_table->columnCount + 1].string;
			return unlock_type && std::strcmp(unlock_type, "loot") != 0;
		}

		bool is_owned_mp_loot(const int controller, const unsigned int item_id,
			const game::StringTable* table, const int row)
		{
			if (controller != 0 || !item_id || game::environment::is_zombies() ||
				!table || !table->values || row < 0 || row >= table->rowCount || table->columnCount <= 1)
				return false;
			const auto* type = table->values[static_cast<std::size_t>(row) * table->columnCount + 1].string;
			if (!type || std::strcmp(type, "loot") != 0) return false;
			// GetItemLockState (CF850 -> D0B10) and IsGuidUnlocked (73F9F0 -> D1050)
			// share these unlock-table checks. An owned loot row must use the same native
			// inventory/expiry predicate as Inventory_IsItemGUIDUsableForPlayer. This is
			// the cache filled by task 165 and HQ purchases/drops, not a second inventory.
			if (!*reinterpret_cast<const unsigned char*>(0x80385A8_g)) return false;
			// Mirror 27A310 after its unlock-all shortcut; do not recurse through D0980.
			const unsigned* item{};
			const auto slot = utils::hook::invoke<unsigned short>(0x279300_g, controller, item_id, &item);
			return item && item[1] != 0 && !utils::hook::invoke<bool>(0x27A260_g, controller, slot);
		}

		int live_storage_is_item_unlocked_from_table_stub(const unsigned int item_id, const int controller_index,
			void* stats_source, void* stats_buffer, game::StringTable* unlock_table, const int row, void* out_param)
		{
			const auto overridden = unlock_all_enabled() && is_normal_unlock(unlock_table, row);
			record_call(table_calls, overridden, unlock_table);

			if (overridden)
			{
				return 0;
			}

			if (is_owned_mp_loot(controller_index, item_id, unlock_table, row)) return 0;

			return live_storage_is_item_unlocked_from_table_hook.invoke<int>(item_id, controller_index, stats_source,
				stats_buffer, unlock_table, row, out_param);
		}

		int live_storage_is_item_unlocked_from_table_local_client_stub(const unsigned int local_client_num,
			game::StringTable* unlock_table, const int row, const unsigned int item_id)
		{
			const auto overridden = unlock_all_enabled() && is_normal_unlock(unlock_table, row);
			record_call(local_client_calls, overridden, unlock_table);

			if (overridden)
			{
				return 0;
			}

			if (local_client_num == 0 && is_owned_mp_loot(0, item_id, unlock_table, row)) return 0;

			return live_storage_is_item_unlocked_from_table_local_client_hook.invoke<int>(local_client_num,
				unlock_table, row, item_id);
		}

		int item_unlocked()
		{
			return 0;
		}
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated())
			{
				utils::hook::jump(0xD0B10_g, item_unlocked);
				utils::hook::jump(0xD1050_g, item_unlocked);
				return;
			}

			cg_unlock_all_items = game::Dvar_RegisterBool("cg_unlockall_items", false, game::DVAR_FLAG_SAVED);
			cg_unlock_debug = game::Dvar_RegisterBool("cg_unlock_debug", false, game::DVAR_FLAG_NONE);

			live_storage_is_item_unlocked_from_table_hook.create(0xD0B10_g,
				live_storage_is_item_unlocked_from_table_stub);
			live_storage_is_item_unlocked_from_table_local_client_hook.create(0xD1050_g,
				live_storage_is_item_unlocked_from_table_local_client_stub);

			scheduler::loop(report_calls, scheduler::pipeline::main, 2s);
		}
	};
}

REGISTER_COMPONENT(unlock_items::component)
