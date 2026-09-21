#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "command.hpp"
#include "console/console.hpp"
#include "unlock_zombies.hpp"

#include "game/game.hpp"
#include "game/demonware/hq_economy.hpp"
#include "game/ui_scripting/execution.hpp"

#include "ui_scripting.hpp"

#include <algorithm>
#include <charconv>
#include <optional>

namespace stats
{
	namespace
	{
		constexpr unsigned int ranked_stats_group = 0;
		constexpr unsigned int stats_group_count = 10;
		constexpr std::size_t max_stat_path_elements = 16;
		// Division levels are stored zero-based: rank 3 is displayed as level 4.
		constexpr int max_division_rank = 3;
		constexpr int max_division_prestige = 4;
		constexpr int max_weapon_prestige = 4;
		constexpr int challenge_target_column = 9;
		constexpr int max_challenge_tiers = 9;

		struct alignas(16) ddl_state
		{
			std::byte data[32]{};
		};

		struct weapon_progress
		{
			int level{};
			int experience{};
		};

		bool parse_integer(const char* text, int& value)
		{
			if (!text || !*text)
			{
				return false;
			}

			const auto* end = text + std::strlen(text);
			const auto result = std::from_chars(text, end, value);
			return result.ec == std::errc{} && result.ptr == end;
		}

		const char* get_cell(const game::StringTable* table, const int row, const int column)
		{
			if (!table || !table->values || row < 0 || row >= table->rowCount || column < 0 ||
				column >= table->columnCount)
			{
				return nullptr;
			}

			return table->values[row * table->columnCount + column].string;
		}

		game::StringTable* find_string_table(const char* name)
		{
			return game::DB_FindXAssetHeader(game::ASSET_TYPE_STRINGTABLE, name, false).stringTable;
		}

		int find_row(const game::StringTable* table, const int column, const std::string_view value)
		{
			const std::string needle{value};

			for (auto row = 0; table && row < table->rowCount; ++row)
			{
				const auto* cell = get_cell(table, row, column);
				if (cell && _stricmp(cell, needle.c_str()) == 0)
				{
					return row;
				}
			}

			return -1;
		}

		bool get_integer_cell(const game::StringTable* table, const int row, const int column, int& value)
		{
			return parse_integer(get_cell(table, row, column), value);
		}

		bool has_stats()
		{
			const auto controller_index = game::CL_ControllerIndexFromClientNum(0);
			return controller_index >= 0 && game::LiveStorage_DoWeHaveStats(controller_index);
		}

		bool is_stat_path_valid(const unsigned int* path, const std::size_t path_count,
			const unsigned int stats_group)
		{
			if (!path || path_count == 0 || path_count > max_stat_path_elements)
			{
				return false;
			}

			const auto* definition = game::LiveStorage_GetStatsGroupDDLDefinition(stats_group);
			if (!definition)
			{
				return false;
			}

			ddl_state state{};
			game::DDL_InitState(definition, &state, stats_group);

			if (!game::DDL_MoveToPath(&state, &state, static_cast<int>(path_count), path))
			{
				return false;
			}

			return game::DDL_GetType(&state) <= 3;
		}

		bool set_stat(const unsigned int* path, const std::size_t path_count, const int value,
			const unsigned int stats_group)
		{
			const auto controller_index = game::CL_ControllerIndexFromClientNum(0);
			if (controller_index < 0 || !game::LiveStorage_DoWeHaveStats(controller_index) || !path ||
				path_count == 0 || path_count > std::numeric_limits<unsigned int>::max() ||
				!is_stat_path_valid(path, path_count, stats_group))
			{
				return false;
			}

			if (!game::LiveStorage_PlayerDataSetIntByNameArray(controller_index, path,
				static_cast<unsigned int>(path_count), value, stats_group))
			{
				return false;
			}

			game::LiveStorage_StatsWriteNeeded(controller_index);
			return true;
		}

		bool set_ranked_stat(const unsigned int* path, const std::size_t path_count, const int value)
		{
			return set_stat(path, path_count, value, ranked_stats_group);
		}

		bool set_stat(const std::initializer_list<std::string_view> path, const int value,
			const unsigned int stats_group)
		{
			std::vector<unsigned int> hashed_path{};
			hashed_path.reserve(path.size());

			for (const auto token : path)
			{
				if (token.empty())
				{
					return false;
				}

				const std::string null_terminated_token{token};
				hashed_path.emplace_back(game::DDL_HashString(null_terminated_token.c_str()));
			}

			return set_stat(hashed_path.data(), hashed_path.size(), value, stats_group);
		}

		bool set_ranked_stat(const std::initializer_list<std::string_view> path, const int value)
		{
			return set_stat(path, value, ranked_stats_group);
		}

		bool is_stat_path_valid(const std::initializer_list<std::string_view> path,
			const unsigned int stats_group)
		{
			std::vector<unsigned int> hashed_path{};
			hashed_path.reserve(path.size());

			for (const auto token : path)
			{
				if (token.empty())
				{
					return false;
				}

				const std::string null_terminated_token{token};
				hashed_path.emplace_back(game::DDL_HashString(null_terminated_token.c_str()));
			}

			return is_stat_path_valid(hashed_path.data(), hashed_path.size(), stats_group);
		}

		bool find_zombie_stats_group(unsigned int& stats_group)
		{
			for (auto candidate = 0u; candidate < stats_group_count; ++candidate)
			{
				if (is_stat_path_valid({"prestigeLevel"}, candidate) &&
					is_stat_path_valid({"totalXP"}, candidate))
				{
					stats_group = candidate;
					return true;
				}
			}

			return false;
		}

		bool set_ranked_stat_with_leaf(const std::initializer_list<std::string_view> parent_path,
			const std::initializer_list<std::string_view> leaf_candidates, const int value)
		{
			std::vector<unsigned int> hashed_path{};
			hashed_path.reserve(parent_path.size() + 1);

			for (const auto token : parent_path)
			{
				if (token.empty())
				{
					return false;
				}

				const std::string null_terminated_token{token};
				hashed_path.emplace_back(game::DDL_HashString(null_terminated_token.c_str()));
			}

			for (const auto leaf : leaf_candidates)
			{
				const std::string null_terminated_leaf{leaf};
				hashed_path.emplace_back(game::DDL_HashString(null_terminated_leaf.c_str()));

				if (set_ranked_stat(hashed_path.data(), hashed_path.size(), value))
				{
					return true;
				}

				hashed_path.pop_back();
			}

			return false;
		}

		bool get_rank_caps(const char* table_name, int& max_prestige, int& max_experience)
		{
			const auto* rank_table = find_string_table(table_name);
			if (!rank_table)
			{
				return false;
			}

			const auto max_prestige_row = find_row(rank_table, 0, "maxprestige");
			const auto final_rank_row = find_row(rank_table, 0, "maxrankfinalprestige");
			int final_rank{};

			if (!get_integer_cell(rank_table, max_prestige_row, 1, max_prestige) ||
				!get_integer_cell(rank_table, final_rank_row, 1, final_rank))
			{
				return false;
			}

			const auto experience_row = find_row(rank_table, 0, std::to_string(final_rank));
			return get_integer_cell(rank_table, experience_row, 7, max_experience);
		}

		void add_weapon_progress(const game::StringTable* table,
			std::map<std::string, weapon_progress>& weapons,
			const int level_cap = std::numeric_limits<int>::max())
		{
			for (auto row = 0; table && row < table->rowCount; ++row)
			{
				const auto* weapon = get_cell(table, row, 0);
				int max_level{};

				if (!weapon || !*weapon || !get_integer_cell(table, row, 1, max_level) || max_level <= 0 ||
					max_level > table->rowCount - row - 1)
				{
					continue;
				}

				const auto target_level = std::min(max_level, level_cap);
				int max_experience{};
				if (target_level <= 0 ||
					!get_integer_cell(table, row + target_level, 1, max_experience) || max_experience < 0 ||
					max_experience == std::numeric_limits<int>::max())
				{
					continue;
				}

				auto& progress = weapons[weapon];
				if (target_level > progress.level ||
					(target_level == progress.level && max_experience + 1 > progress.experience))
				{
					progress.level = target_level;
					// The stock rank-up path stores one past the threshold for the current level.
					progress.experience = max_experience + 1;
				}
			}
		}

		std::map<std::string, weapon_progress> get_weapon_progress()
		{
			std::map<std::string, weapon_progress> weapons{};

			add_weapon_progress(find_string_table("mp/weaponLeveling.csv"), weapons);
			add_weapon_progress(find_string_table("mp/weaponLevelingDivisionsOverhaul.csv"), weapons);

			return weapons;
		}

		weapon_progress get_division_progress()
		{
			std::map<std::string, weapon_progress> entries{};
			const auto* overhaul_table = find_string_table("mp/divisionLevelingOverhaul.csv");
			const auto* original_table = find_string_table("mp/divisionLeveling.csv");

			add_weapon_progress(overhaul_table, entries, max_division_rank);
			add_weapon_progress(original_table, entries, max_division_rank);

			weapon_progress result{};
			for (const auto& [name, progress] : entries)
			{
				(void)name;

				if (progress.level > max_division_rank)
				{
					continue;
				}

				if (progress.level > result.level ||
					(progress.level == result.level && progress.experience > result.experience))
				{
					result = progress;
				}
			}

			for (const auto* table : {overhaul_table, original_table})
			{
				for (auto row = 0; table && row < table->rowCount; ++row)
				{
					int level{};
					int experience{};
					if (!get_integer_cell(table, row, 0, level) ||
						!get_integer_cell(table, row, 1, experience) || level <= 0 ||
						level > max_division_rank ||
						experience < 0 || experience == std::numeric_limits<int>::max())
					{
						continue;
					}

					if (level > result.level || (level == result.level && experience + 1 > result.experience))
					{
						result.level = level;
						result.experience = experience + 1;
					}
				}
			}

			return result;
		}

		std::vector<std::string> get_weapon_stat_names(const std::string& weapon)
		{
			std::vector<std::string> names{weapon};
			constexpr std::string_view mp_suffix{"_mp"};

			if (weapon.size() > mp_suffix.size() &&
				weapon.compare(weapon.size() - mp_suffix.size(), mp_suffix.size(), mp_suffix) == 0)
			{
				names.emplace_back(weapon.substr(0, weapon.size() - mp_suffix.size()));
			}
			else
			{
				names.emplace_back(weapon + std::string{mp_suffix});
			}

			return names;
		}

		bool set_weapon_stat(const std::string& weapon,
			const std::initializer_list<std::string_view> leaf_candidates, const int value)
		{
			for (const auto& stat_name : get_weapon_stat_names(weapon))
			{
				if (set_ranked_stat_with_leaf({"weaponStats", stat_name}, leaf_candidates, value))
				{
					return true;
				}
			}

			return false;
		}

		std::pair<std::size_t, std::size_t> unlock_challenges()
		{
			const auto* table = find_string_table("mp/allChallengesTable.csv");
			std::size_t updated{};
			std::size_t total{};

			for (auto row = 0; table && row < table->rowCount; ++row)
			{
				const auto* challenge = get_cell(table, row, 0);
				if (!challenge || !*challenge)
				{
					continue;
				}

				++total;

				int max_state{};
				int max_progress{};
				for (auto tier = 0; tier < max_challenge_tiers; ++tier)
				{
					int progress{};
					if (!get_integer_cell(table, row, challenge_target_column + tier * 2, progress) ||
						progress == 0)
					{
						break;
					}

					// State 1 is the active first tier; completion begins at state 2.
					max_state = tier + 2;
					max_progress = progress == std::numeric_limits<int>::min()
						? std::numeric_limits<int>::max()
						: std::abs(progress);
				}

				if (max_state == 0)
				{
					continue;
				}

				const auto state_set = set_ranked_stat({"challengeState", challenge}, max_state);
				const auto progress_set = set_ranked_stat({"challengeProgress", challenge}, max_progress);
				if (state_set && progress_set)
				{
					++updated;
				}
			}

			return {updated, total};
		}

		void set_player_data_int(const command::params& params)
		{
			if (params.size() < 3)
			{
				console::info("Usage: setPlayerDataInt <path...> <value>\n");
				return;
			}

			int value{};
			if (!parse_integer(params[params.size() - 1], value))
			{
				console::error("setPlayerDataInt: '%s' is not a valid integer.\n",
					params[params.size() - 1]);
				return;
			}

			if (!has_stats())
			{
				console::error("setPlayerDataInt: player stats are not available.\n");
				return;
			}

			if (params.size() - 2 > static_cast<int>(max_stat_path_elements))
			{
				console::error("setPlayerDataInt: paths are limited to 16 elements.\n");
				return;
			}

			std::vector<unsigned int> path{};
			path.reserve(params.size() - 2);

			for (auto index = 1; index < params.size() - 1; ++index)
			{
				path.emplace_back(game::DDL_HashString(params[index]));
			}

			if (!set_ranked_stat(path.data(), path.size(), value))
			{
				console::error("setPlayerDataInt: invalid ranked stat path or value.\n");
				return;
			}

			console::info("setPlayerDataInt: stat updated.\n");
		}

		void unlock_multiplayer_stats(const command::params& params)
		{
			if (params.size() != 2 || std::string_view{params[1]} != "confirm")
			{
				console::warn("unlockstatsmp: this permanently changes Multiplayer progression and stats "
					"and cannot be automatically undone. Run \"unlockstatsmp confirm\" to continue.\n");
				return;
			}

			if (!game::environment::is_multiplayer())
			{
				console::error("unlockstatsmp: this command is only available in Multiplayer.\n");
				return;
			}

			if (!has_stats())
			{
				console::error("unlockstatsmp: player stats are not available.\n");
				return;
			}

			bool rank_unlocked{};
			int max_prestige{};
			int max_experience{};
			if (get_rank_caps("mp/rankTable.csv", max_prestige, max_experience))
			{
				const auto prestige_set = set_ranked_stat({"prestige"}, max_prestige);
				const auto experience_set = set_ranked_stat({"experience"}, max_experience);
				rank_unlocked = prestige_set && experience_set;
			}

			const auto weapons = get_weapon_progress();
			std::size_t unlocked_weapons{};
			std::size_t prestiged_weapons{};

			for (const auto& [weapon, progress] : weapons)
			{
				const auto prestige_set = set_weapon_stat(weapon,
					{"prestige", "weaponPrestige", "prestigeLevel"}, max_weapon_prestige);
				const auto level_set = set_weapon_stat(weapon,
					{"level", "weaponRank", "weaponLevel", "rank"}, progress.level);
				const auto experience_set = set_weapon_stat(weapon, {"experience", "xp"},
					progress.experience);

				if (prestige_set)
				{
					++prestiged_weapons;
				}

				if (level_set && experience_set)
				{
					++unlocked_weapons;
				}
			}

			constexpr std::array divisions
			{
				"infantry", "airborne", "armored", "mountain", "expeditionary", "resistance",
				"cavalry", "grenadier", "commando", "scout", "artillery"
			};

			const auto division_progress = get_division_progress();
			std::size_t unlocked_divisions{};
			std::size_t prestiged_divisions{};
			if (division_progress.level > 0)
			{
				for (const auto division : divisions)
				{
					const auto prestige_set = set_ranked_stat_with_leaf({"divisionStats", division},
						{"prestigeLevel", "prestige"}, max_division_prestige);

					const auto level_set = set_ranked_stat_with_leaf({"divisionStats", division},
						{"level", "divisionLevel"}, division_progress.level);
					const auto experience_set = set_ranked_stat_with_leaf({"divisionStats", division},
						{"experience", "xp", "divisionXP"}, division_progress.experience);

					if (prestige_set)
					{
						++prestiged_divisions;
					}

					if (level_set && experience_set)
					{
						++unlocked_divisions;
					}
				}
			}

			const auto [unlocked_challenges, challenges] = unlock_challenges();

			console::debug("unlockstatsmp: rank progression updated: %s.\n",
				rank_unlocked ? "yes" : "no");
			console::debug("unlockstatsmp: %zu of %zu weapon progression entries updated.\n",
				unlocked_weapons, weapons.size());
			console::debug("unlockstatsmp: %zu weapon prestige entries updated.\n",
				prestiged_weapons);
			console::debug("unlockstatsmp: %zu division progression entries updated.\n",
				unlocked_divisions);
			console::debug("unlockstatsmp: %zu division prestige entries updated.\n",
				prestiged_divisions);
			console::debug("unlockstatsmp: %zu of %zu challenge entries updated.\n",
				unlocked_challenges, challenges);

			if (!rank_unlocked)
			{
				console::warn("unlockstatsmp: failed to update rank progression.\n");
			}

			if (weapons.empty())
			{
				console::warn("unlockstatsmp: no weapon-leveling table was available.\n");
			}
			else if (unlocked_weapons == 0)
			{
				console::warn("unlockstatsmp: no compatible weapon progression entries were found.\n");
			}

			if (!weapons.empty() && prestiged_weapons == 0)
			{
				console::warn("unlockstatsmp: no compatible weapon prestige entries were found.\n");
			}

			if (division_progress.level <= 0)
			{
				console::warn("unlockstatsmp: no division-leveling table was available.\n");
			}
			else if (unlocked_divisions == 0)
			{
				console::warn("unlockstatsmp: no compatible division progression entries were found.\n");
			}

			if (division_progress.level > 0 && prestiged_divisions == 0)
			{
				console::warn("unlockstatsmp: no compatible division prestige entries were found.\n");
			}

			if (challenges == 0)
			{
				console::warn("unlockstatsmp: no challenge table was available.\n");
			}
			else if (unlocked_challenges == 0)
			{
				console::warn("unlockstatsmp: no compatible challenge entries were found.\n");
			}

			if (rank_unlocked && unlocked_weapons > 0 && prestiged_weapons > 0 &&
				unlocked_divisions > 0 && prestiged_divisions > 0 && unlocked_challenges > 0)
			{
				console::info("unlockstatsmp: Multiplayer progression and challenges unlocked.\n");
			}
		}

		void unlock_zombie_stats(const command::params& params)
		{
			if (params.size() != 2 || std::string_view{params[1]} != "confirm")
			{
				console::warn("unlockstatszm: this permanently changes Zombies progression, including rank "
					"and Hidden Challenges, and cannot be automatically undone. Run \"unlockstatszm confirm\" "
					"to continue.\n");
				return;
			}

			if (!game::environment::is_zombies())
			{
				console::error("unlockstatszm: this command is only available in Zombies.\n");
				return;
			}

			bool rank_unlocked{};
			if (has_stats())
			{
				int max_prestige{};
				int max_experience{};
				unsigned int stats_group{};
				if (get_rank_caps("mp/cp_rankTable.csv", max_prestige, max_experience) &&
					find_zombie_stats_group(stats_group))
				{
					const auto prestige_set = set_stat({"prestigeLevel"}, max_prestige, stats_group);
					const auto experience_set = set_stat({"totalXP"}, max_experience, stats_group);
					rank_unlocked = prestige_set && experience_set;
				}
			}

			console::debug("unlockstatszm: rank progression updated: %s.\n", rank_unlocked ? "yes" : "no");

			if (!rank_unlocked)
			{
				console::warn("unlockstatszm: failed to update Zombies rank progression.\n");
			}

			const auto challenges = unlock_zombies::unlock_hidden_challenges();
			console::debug("unlockstatszm: %d of %d Zombies achievement entries completed.\n",
				challenges.completed, challenges.total);

			const auto challenges_unlocked = challenges.persisted && challenges.total > 0 &&
				challenges.completed == challenges.total;

			if (!challenges.persisted)
			{
				console::warn("unlockstatszm: failed to persist Zombies Hidden Challenge progression.\n");
			}
			else if (!challenges_unlocked)
			{
				console::warn("unlockstatszm: some Zombies Hidden Challenge definitions could not be resolved.\n");
			}

			if (rank_unlocked && challenges_unlocked)
			{
				console::info("unlockstatszm: Zombies progression and Hidden Challenges unlocked.\n");
			}
		}

		void unlock_zombie_easter_eggs(const command::params& params)
		{
			if (params.size() != 2 || std::string_view{params[1]} != "confirm")
			{
				console::warn("unlockzmeastereggs: this permanently marks the Zombies main quests (Tortured Path "
					"chapters and Easter eggs) as completed and cannot be automatically undone. Run "
					"\"unlockzmeastereggs confirm\" to continue.\n");
				return;
			}

			if (!game::environment::is_zombies())
			{
				console::error("unlockzmeastereggs: this command is only available in Zombies.\n");
				return;
			}

			const auto result = unlock_zombies::unlock_easter_eggs();
			if (!result.persisted)
			{
				console::warn("unlockzmeastereggs: failed to persist the Zombies main quest progression.\n");
				return;
			}

			if (result.completed != result.total)
			{
				console::warn("unlockzmeastereggs: %d of %d main quest entries could not be resolved.\n",
					result.total - result.completed, result.total);
			}

			console::info("unlockzmeastereggs: %d Zombies main quest entries marked as completed. "
				"Return to the lobby for the change to apply.\n", result.completed);
		}
	}

	namespace
	{
		// Rank tables (mp/rankTable.csv, mp/cp_rankTable.csv) key their rows by
		// the zero-based rank index; column 2 holds the rank's minimum experience.
		// "maxrank" is the highest rank index before the final prestige and
		// "maxrankfinalprestige" the highest index at master prestige.
		constexpr int rank_min_experience_column = 2;

		struct rank_table_info
		{
			const game::StringTable* table{};
			int max_prestige{};
			int max_rank_index{};
			int max_rank_index_final_prestige{};
			// Minimum experience per rank index, 0..max_rank_index_final_prestige,
			// checked at load to be complete and non-decreasing so the lookups below
			// never stop early on a missing or malformed row.
			std::vector<int> minimum_experience{};
		};

		bool load_rank_table(const char* table_name, rank_table_info& info)
		{
			info = {};
			info.table = find_string_table(table_name);
			if (!info.table)
			{
				return false;
			}

			if (!get_integer_cell(info.table, find_row(info.table, 0, "maxprestige"), 1, info.max_prestige) ||
				!get_integer_cell(info.table, find_row(info.table, 0, "maxrankfinalprestige"), 1,
					info.max_rank_index_final_prestige))
			{
				return false;
			}

			// The caps feed std::clamp and a + 1 level conversion, so a malformed
			// table must be rejected here rather than produce reversed bounds or an
			// overflow later. Every rank index needs a row of its own, which bounds
			// the final cap by the table's row count. A missing maxrank row is a
			// supported layout (the regular cap equals the final-prestige cap); a
			// present but invalid cell is not.
			if (info.max_prestige < 0 || info.max_rank_index_final_prestige < 0 ||
				info.max_rank_index_final_prestige >= info.table->rowCount)
			{
				return false;
			}

			const auto max_rank_row = find_row(info.table, 0, "maxrank");
			if (max_rank_row < 0)
			{
				info.max_rank_index = info.max_rank_index_final_prestige;
			}
			else if (!get_integer_cell(info.table, max_rank_row, 1, info.max_rank_index) ||
				info.max_rank_index < 0 || info.max_rank_index > info.max_rank_index_final_prestige)
			{
				return false;
			}

			// Collect the minimum experience of every rank the caps can name. A rank
			// that is missing, listed twice, or carries a non-numeric or negative XP
			// cell rejects the table, as does experience that decreases from one rank
			// to the next: the level conversion walks the ranks in order and a rank
			// row must never be mistaken for the threshold above the player's XP.
			const auto rank_count = static_cast<std::size_t>(info.max_rank_index_final_prestige) + 1;
			std::vector<int> minimum_experience(rank_count);
			std::vector<bool> seen(rank_count);
			for (auto row = 0; row < info.table->rowCount; ++row)
			{
				int rank_index{};
				if (!get_integer_cell(info.table, row, 0, rank_index) || rank_index < 0 ||
					static_cast<std::size_t>(rank_index) >= rank_count)
				{
					continue;
				}

				int minimum{};
				if (seen[rank_index] ||
					!get_integer_cell(info.table, row, rank_min_experience_column, minimum) || minimum < 0)
				{
					return false;
				}

				seen[rank_index] = true;
				minimum_experience[rank_index] = minimum;
			}

			if (std::find(seen.begin(), seen.end(), false) != seen.end() || minimum_experience[0] != 0 ||
				!std::is_sorted(minimum_experience.begin(), minimum_experience.end()))
			{
				return false;
			}

			info.minimum_experience = std::move(minimum_experience);
			return true;
		}

		int get_rank_level_cap(const rank_table_info& info, const int prestige)
		{
			const auto max_index = prestige >= info.max_prestige
				? info.max_rank_index_final_prestige
				: info.max_rank_index;
			return max_index + 1;
		}

		bool get_rank_experience(const rank_table_info& info, const int level, int& experience)
		{
			if (level < 1 || static_cast<std::size_t>(level) > info.minimum_experience.size())
			{
				return false;
			}

			experience = info.minimum_experience[static_cast<std::size_t>(level) - 1];
			return true;
		}

		// The highest level whose minimum experience the player has reached. Level 1
		// needs zero experience, so a loaded table always yields at least 1.
		int get_level_for_experience(const rank_table_info& info, const int experience)
		{
			auto level = 1;
			for (std::size_t index = 0; index < info.minimum_experience.size(); ++index)
			{
				if (info.minimum_experience[index] > experience)
				{
					break;
				}

				level = static_cast<int>(index) + 1;
			}

			return level;
		}

		const char* get_rank_table_name()
		{
			return game::environment::is_zombies() ? "mp/cp_rankTable.csv" : "mp/rankTable.csv";
		}

		struct progression_target
		{
			const char* table{};
			const char* prestige_stat{};
			const char* experience_stat{};
			unsigned int stats_group{};
		};

		// Resolves the table, stat fields and stats group the current mode's
		// progression lives in. Failures are reported under `command` when one is
		// given; the rank chooser's bridge passes none and only gets false.
		bool resolve_progression_target(const char* command, progression_target& target)
		{
			if (!has_stats())
			{
				if (command)
				{
					console::error("%s: player stats are not available.\n", command);
				}

				return false;
			}

			if (game::environment::is_multiplayer())
			{
				target = {"mp/rankTable.csv", "prestige", "experience", ranked_stats_group};
				return true;
			}

			unsigned int zombie_group{};
			if (game::environment::is_zombies() && find_zombie_stats_group(zombie_group))
			{
				target = {"mp/cp_rankTable.csv", "prestigeLevel", "totalXP", zombie_group};
				return true;
			}

			if (command)
			{
				console::error("%s: only available in Multiplayer or Zombies.\n", command);
			}

			return false;
		}

		// The engine only exposes player stats to LUI, so the current prestige is read
		// through Engine.GetPlayerData - the same call the UNLOCKS rank chooser uses to
		// seed its steppers. Returns false when LUI is down or the field cannot be read.
		bool read_current_prestige(const progression_target& target, int& prestige)
		{
			const auto controller_index = game::CL_ControllerIndexFromClientNum(0);
			if (controller_index < 0 || !*game::hks::lui_lua_state)
			{
				return false;
			}

			auto found = false;
			game::LUI_EnterCriticalSection();

			try
			{
				const auto engine = ui_scripting::get_globals().get("Engine");
				if (engine.is<ui_scripting::table>())
				{
					const auto reader = engine.as<ui_scripting::table>().get("GetPlayerData");
					if (reader.is<ui_scripting::function>())
					{
						const auto result = reader.as<ui_scripting::function>().call(
							{controller_index, static_cast<int>(target.stats_group), target.prestige_stat});
						if (!result.empty() && result[0].is<float>())
						{
							prestige = static_cast<int>(result[0].as<float>());
							found = true;
						}
					}
				}
			}
			catch (const std::exception& e)
			{
				console::debug("failed to read %s from LUI: %s\n", target.prestige_stat, e.what());
			}

			game::LUI_LeaveCriticalSection();
			return found;
		}

		bool apply_progression(const char* command, const std::optional<int> prestige, const int level)
		{
			progression_target target{};
			rank_table_info info{};
			if (!resolve_progression_target(command, target))
			{
				return false;
			}

			if (!load_rank_table(target.table, info))
			{
				console::error("%s: rank table %s is unavailable or has an unexpected layout.\n", command, target.table);
				return false;
			}

			// Levels above the regular cap only exist at the final prestige. With a
			// prestige argument the level is capped for that prestige; without one it is
			// capped for the prestige the player is on right now. When the current
			// prestige cannot be read, cap as at prestige 0 and say so - the two-argument
			// form and setprestige still reach every level.
			auto current_prestige = 0;
			if (!prestige && !read_current_prestige(target, current_prestige))
			{
				console::warn("%s: your current prestige could not be read; capping the level as at prestige 0. "
					"Pass a prestige (%s <level> <prestige>) to reach the levels above that cap.\n",
					command, command);
			}

			const auto chosen_prestige = std::clamp(prestige ? *prestige : current_prestige, 0, info.max_prestige);
			const auto level_cap = get_rank_level_cap(info, chosen_prestige);
			const auto chosen_level = std::clamp(level, 1, level_cap);

			int experience{};
			if (!get_rank_experience(info, chosen_level, experience))
			{
				console::error("%s: no rank row for level %d in %s.\n", command, chosen_level, target.table);
				return false;
			}

			// MP displays experience + inventoryTotalXP - inventoryXPAtLastReset
			// (native 0xD71B0). Setting only experience leaves past reward XP on top
			// of the requested rank. Match the native prestige reset (0x18ED20) by
			// baselining both inventory fields to currency 1. Use the persisted wallet
			// that hq_native projects, so a pending wallet refresh cannot restore old XP.
			int inventory_experience{};
			const auto reset_inventory_experience = target.stats_group == ranked_stats_group;
			if (reset_inventory_experience)
			{
				try
				{
					const auto wallet = demonware::hq_economy::snapshot();
					const auto it = wallet.currencies.find(1);
					const auto amount = it == wallet.currencies.end() ? 0u : it->second;
					if (amount > static_cast<unsigned>(std::numeric_limits<int>::max()) ||
						!is_stat_path_valid({"inventoryTotalXP"}, target.stats_group) ||
						!is_stat_path_valid({"inventoryXPAtLastReset"}, target.stats_group))
					{
						console::error("%s: inventory XP cannot be reset safely; rank was not changed.\n", command);
						return false;
					}
					inventory_experience = static_cast<int>(amount);
				}
				catch (const std::exception& error)
				{
					console::error("%s: cannot read inventory XP: %s. Rank was not changed.\n", command, error.what());
					return false;
				}
			}

			// The game's own prestige routine advances by exactly one prestige and only
			// in Multiplayer, so it cannot serve a command that sets an absolute prestige
			// in either mode. We write the prestige stat directly instead. That is the
			// prestige the menus read; it is not the retail prestige event, so the
			// cosmetic rewards that normally come with a prestige are not granted.
			if (prestige && !set_stat({target.prestige_stat}, chosen_prestige, target.stats_group))
			{
				console::error("%s: failed to write %s.\n", command, target.prestige_stat);
				return false;
			}

			if (!set_stat({target.experience_stat}, experience, target.stats_group))
			{
				console::error("%s: failed to write %s.\n", command, target.experience_stat);
				return false;
			}
			if (reset_inventory_experience &&
				(!set_stat({"inventoryTotalXP"}, inventory_experience, target.stats_group) ||
					!set_stat({"inventoryXPAtLastReset"}, inventory_experience, target.stats_group)))
			{
				console::error("%s: failed to reset inventory XP; rank update is incomplete.\n", command);
				return false;
			}

			if (prestige)
			{
				console::info("%s: prestige %d, level %d applied (%s = %d). Re-open the Soldier menu to refresh the display.\n",
					command, chosen_prestige, chosen_level, target.experience_stat, experience);
				console::info("%s: this sets the prestige and level only; the rewards a prestige normally grants are not given.\n", command);
			}
			else
			{
				console::info("%s: level %d applied (%s = %d). Re-open the Soldier menu to refresh the display.\n",
					command, chosen_level, target.experience_stat, experience);
			}

			if (prestige && (chosen_prestige != *prestige || chosen_level != level))
			{
				console::warn("%s: values were clamped to prestige 0-%d and level 1-%d.\n",
					command, info.max_prestige, level_cap);
			}
			else if (!prestige && chosen_level != level)
			{
				console::warn("%s: values were clamped to level 1-%d at prestige %d.\n",
					command, level_cap, chosen_prestige);
			}

			return true;
		}

		void set_rank_command(const command::params& params)
		{
			int level{};
			int prestige{};
			if (params.size() < 2 || params.size() > 3 || !parse_integer(params[1], level) ||
				(params.size() == 3 && !parse_integer(params[2], prestige)))
			{
				console::info("Usage: setrank <level> [prestige]\n");
				console::info("Without a prestige the level is capped at your current prestige's maximum.\n");
				return;
			}

			apply_progression("setrank", params.size() == 3 ? std::optional{prestige} : std::nullopt, level);
		}

		void set_prestige_command(const command::params& params)
		{
			int prestige{};
			if (params.size() != 2 || !parse_integer(params[1], prestige))
			{
				console::info("Usage: setprestige <prestige>\n");
				console::info("Sets the prestige and level the menus show. It does not grant the prestige rewards.\n");
				return;
			}

			apply_progression("setprestige", prestige, 1);
		}

		// Lua helpers for the UNLOCKS tab rank chooser.
		void install_lua_functions()
		{
			auto lua = ui_scripting::get_globals();
			ui_scripting::table stats_table{};
			lua["S2xStats"] = stats_table;

			// maxPrestige, maxLevel (before the final prestige), maxLevelFinalPrestige;
			// nothing when the current mode's rank table is unavailable or malformed,
			// so the chooser cannot mistake a default for a cap.
			stats_table["GetRankCaps"] = []() -> ui_scripting::arguments
			{
				rank_table_info info{};
				if (!load_rank_table(get_rank_table_name(), info))
				{
					return {};
				}

				return {info.max_prestige, info.max_rank_index + 1, info.max_rank_index_final_prestige + 1};
			};

			// The level a total experience value has reached; nothing when the table
			// cannot be used.
			stats_table["GetLevelForExperience"] = [](const int experience) -> ui_scripting::arguments
			{
				rank_table_info info{};
				if (!load_rank_table(get_rank_table_name(), info))
				{
					return {};
				}

				return {get_level_for_experience(info, std::max(experience, 0))};
			};

			// statsGroup, prestigeField, experienceField: the stats setrank and
			// setprestige write, so the chooser seeds itself from the same fields the
			// commands change. Nothing until the player's stats are loaded.
			stats_table["GetProgressionSource"] = []() -> ui_scripting::arguments
			{
				progression_target target{};
				if (!resolve_progression_target(nullptr, target))
				{
					return {};
				}

				return {static_cast<int>(target.stats_group), target.prestige_stat, target.experience_stat};
			};

			stats_table["HasStats"] = []()
			{
				return has_stats();
			};
		}
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			if (game::environment::is_dedicated())
			{
				return;
			}

			command::add("setPlayerDataInt", set_player_data_int);
			command::add("unlockstatsmp", unlock_multiplayer_stats);
			command::add("unlockstatszm", unlock_zombie_stats);
			command::add("setrank", set_rank_command);
			command::add("setprestige", set_prestige_command);
			command::add("unlockzmeastereggs", unlock_zombie_easter_eggs);

			ui_scripting::on_start(install_lua_functions);
		}
	};
}

REGISTER_COMPONENT(stats::component)
