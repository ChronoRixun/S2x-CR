#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "command.hpp"
#include "scheduler.hpp"
#include "scripting.hpp"
#include "party.hpp"

#include "game/game.hpp"

#include "console/console.hpp"

#include <utils/hook.hpp>

#include <algorithm>
#include <random>

namespace bots
{
	namespace
	{
		const game::dvar_t* bot_fill{};
		const game::dvar_t* bot_names_dvar{};
		std::atomic<std::uint32_t> level_generation{0};

		constexpr const char* const bot_names_values[] =
		{
			"default",
			"modern",
			"nostalgia",
			nullptr
		};

		constexpr const char* modern_names[] =
		{
			"Vex", "Nyx", "Kova", "Raze", "Jinx", "Onyx", "Pyke", "Flak",
			"Torq", "Drek",
			"IronWake", "ColdPulse", "StormEdge", "AshenVolt", "HollowPoint",
			"BrokenClock", "StaticWolf", "DeadChannel", "FrostLatch",
			"DarkReach", "SilverMaw", "BlindSprint",
			"Smoke_Trail", "Dead_Freq", "Pale_Nerve", "Grim_Output",
			"Burnt_Signal", "Cold_Ransom",
			"Krypt99", "Havoc04", "Sable17", "Reaver22", "Dusty301",
			"Omen415", "Zenith03", "Harko27", "Dreggs56", "Pyre612",
			"softlock", "voidrun", "bleakwinter", "quietfang", "novawreck",
			"lostfreight",
			"ClutchDeny", "DropShotRex", "HeadGlitch", "OneTapRon",
			"HardScope",
			"MikeFromOhio", "TacoTuesday44", "SleepyDave", "RunningLate",
			"NotGoodAtThis",
		};

		constexpr const char* nostalgia_names[] =
		{
			"xXDarkAngelXx", "XxDeathStrikerxX", "xXBulletStormXx",
			"xXSilentKillaxX", "XxVenom420xX", "xXPhantomZXx",
			"XxDemonSlyrXx", "xXxChaosReapxXx",
			"N00bSl4y3r", "H3adSh0tz", "D3athD34l3r", "Gh0stR1d3r",
			"Pr0Sn1p3z", "R4g3Qu1t",
			"CrimsonEagle47", "SilentFalcon82", "BrokenHammer19",
			"FrozenTiger33", "ArcticBadger61", "ThunderPebble04",
			"WickedMoose77", "AngryWaffle39",
			"DarkAssassin99", "ShadowReaper_", "xDemonHunterx",
			"DeathWish360", "BloodFangz", "NightStalker07",
			"SoulReaver666", "DarkPhoenix187",
			"FaZeScopez", "OpTiCPulse", "Predatr", "Clutchh",
			"sTrIkEz", "QuiKz",
			"IEatBullets", "ICampInCorners", "IDontMissShots",
			"ITrickShot", "IRunWithKnives", "INeverReload",
			"Sniper_Wolf_93", "CoD_K1NG_420", "TrIgGeR_HaPpY",
			"HeadHunter01", "NoSc0pe360", "KillShot_69",
			"iAmLegend", "oG_Gunner", "Captain_Lag", "SirLagsALot",
			"CtrlAltDefeat", "CamperKing2010",
		};

		std::vector<const char*> name_pool{};
		std::size_t name_index{0};

		void shuffle_name_pool()
		{
			name_pool.clear();
			name_index = 0;

			if (!bot_names_dvar)
			{
				return;
			}

			const auto mode = bot_names_dvar->current.integer;

			if (mode == 1)
			{
				name_pool.assign(std::begin(modern_names), std::end(modern_names));
			}
			else if (mode == 2)
			{
				name_pool.assign(std::begin(nostalgia_names), std::end(nostalgia_names));
			}
			else
			{
				return;
			}

			std::mt19937 rng{std::random_device{}()};
			std::shuffle(name_pool.begin(), name_pool.end(), rng);
		}

		utils::hook::detour get_bot_name_hook;

		const char* get_bot_name_stub()
		{
			if (name_pool.empty())
			{
				return get_bot_name_hook.invoke<const char*>();
			}

			return name_pool[name_index++ % name_pool.size()];
		}

		int get_requested_bot_count(const command::params& params)
		{
			if (params.size() <= 1)
			{
				return 1;
			}

			return std::max(1, std::atoi(params[1]));
		}

		bool spawn_bot()
		{
			auto* ent = game::mp::SV_AddBot("", 1);
			if (!ent)
			{
				return false;
			}

			game::mp::SV_SpawnTestClient(ent);
			return true;
		}

		// Returns how many bots the engine actually added.
		int spawn_bots(const int count)
		{
			auto spawned = 0;
			for (int i = 0; i < count; ++i)
			{
				if (spawn_bot())
				{
					++spawned;
				}
			}

			return spawned;
		}

		// Spawns up to the requested number of bots, limited by the free match
		// slots. Returns how many were actually added and reports a shortfall, so
		// the caller never announces more bots than the match received.
		int spawn_bots_capped(const int requested, const char* origin)
		{
			const auto planned = std::min(requested, party::get_available_match_slots());
			if (planned <= 0)
			{
				console::warn("%s: cannot spawn bots, match player limit reached\n", origin);
				return 0;
			}

			const auto spawned = spawn_bots(planned);
			if (spawned < planned)
			{
				console::warn("%s: the engine added %d of %d requested bot(s)\n", origin, spawned, planned);
			}

			return spawned;
		}

		void spawn_bot_command(const command::params& params)
		{
			if (!game::is_server_running())
			{
				return;
			}

			const auto requested = get_requested_bot_count(params);

			scheduler::once([requested]
			{
				const auto spawned = spawn_bots_capped(requested, "spawnBot");
				if (spawned > 0)
				{
					console::info("spawnBot: added %d bot(s)\n", spawned);
				}
			}, scheduler::server);
		}

		int get_fill_target()
		{
			if (!bot_fill || !game::environment::is_multiplayer())
			{
				return 0;
			}

			// Bounded by the slots this server owns, not the mode maximum, so a
			// small server does not chase a target it can never reach.
			return std::clamp(bot_fill->current.integer, 0, party::get_match_capacity());
		}

		// One fill pass. Returns true when the target is met or no more bots
		// can be added; false when the engine refused at least one and a
		// recount on the next server frame is worth trying.
		bool fill_bots()
		{
			const auto target = get_fill_target();
			const auto present = party::get_bot_count();
			const auto missing = target - present;

			if (missing <= 0)
			{
				console::info("bot_fill: %d/%d bots already present\n", present, target);
				return true;
			}

			const auto spawned = spawn_bots_capped(missing, "bot_fill");
			console::info("bot_fill: spawned %d of %d bot(s) (%d present, target %d)\n",
				spawned, missing, present, target);

			return spawned >= missing;
		}

		// SV_AddBot can refuse a bot right after match start (observed: 3 of 4
		// on a dedicated server, seconds after "match started"; later attempts
		// in the same level succeeded at 4/4 and 8/8). The cause has not been
		// identified, but the refusal is transient — retrying on the next
		// server frame is enough. Three extra passes covers every observed
		// case without a blind timer.
		constexpr auto fill_retry_limit = 3;

		// Tops the match up to bot_fill bots. The task runs on the server pipeline,
		// which only ticks once the server is running game frames, and ends if the
		// level goes away under it. On a listen server it also waits for the host to
		// join its own match, the same point at which the manual spawnBot command is
		// known to work.
		void schedule_fill()
		{
			if (get_fill_target() <= 0)
			{
				return;
			}

			const auto generation = level_generation.load();
			const auto dedicated = game::environment::is_dedicated();

			scheduler::schedule([generation, dedicated, attempts = 0, retries = 0]() mutable
			{
				if (generation != level_generation.load() || !game::is_server_running())
				{
					return scheduler::cond_end;
				}

				// Keep waiting for the host: the check is cheap, and the guards above
				// end the wait when the level goes away. A slow load must not lose its
				// bots silently.
				if (!dedicated && !game::CL_IsLocalClientInGame(0))
				{
					if (++attempts == 60)
					{
						console::info("bot_fill: still waiting for the host to join the match\n");
					}

					return scheduler::cond_continue;
				}

				if (fill_bots())
				{
					return scheduler::cond_end;
				}

				if (retries >= fill_retry_limit)
				{
					console::warn("bot_fill: %d/%d bots present after %d recount(s); "
						"the engine is not accepting more right now\n",
						party::get_bot_count(), get_fill_target(), retries);
					return scheduler::cond_end;
				}

				++retries;
				return scheduler::cond_continue;
			}, scheduler::server, 500ms);
		}
	}

	class component final : public multiplayer_component
	{
	public:
		void post_unpack() override
		{
			utils::hook::set(game::BG_BotFastFileEnabled, 0xC301B0);
			utils::hook::set(game::BG_BotsUsingTeamDifficulty, 0xC301B0);
			utils::hook::set(game::BG_BotSystemEnabled, 0xC301B0);
			utils::hook::set(game::BG_AgentSystemEnabled, 0xC301B0);

			// Not sure, is LUA related (Might need additional patches since it also checks OnlineGame dvar outside this function)
			utils::hook::set(0x388210_g, 0xC301B0);

			bot_fill = game::Dvar_RegisterInt("bot_fill", 0, 0,
				game::environment::get_online_mode_info().max_players, game::DVAR_FLAG_SAVED);

			bot_names_dvar = game::Dvar_RegisterEnum("bot_names",
				bot_names_values, 0, game::DVAR_FLAG_SAVED);

			get_bot_name_hook.create(game::mp::SV_BotGetRandomName, get_bot_name_stub);

			command::add("spawnBot", [](const command::params& params)
			{
				spawn_bot_command(params);
			});

			scripting::on_init([]
			{
				++level_generation;
				shuffle_name_pool();
				schedule_fill();
			});

			scripting::on_shutdown([](int)
			{
				++level_generation;
			});
		}
	};
}

REGISTER_COMPONENT(bots::component)
