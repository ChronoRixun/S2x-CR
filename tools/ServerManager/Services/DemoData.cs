using System;
using System.Collections.Generic;
using System.Linq;
using S2x.ServerManager.Models;
using S2x.ServerManager.ViewModels;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// The mockup's demo fleet, so the four states can be looked at without servers.
    /// Used only by --demo; nothing here touches the game folder.
    /// </summary>
    internal static class DemoData
    {
        public static List<KeyValuePair<ServerPreset, ServerState>> Build(string state)
        {
            var fleet = new List<KeyValuePair<ServerPreset, ServerState>>();
            if (string.Equals(state, "empty", StringComparison.OrdinalIgnoreCase)) return fleet;

            var one = Preset("^3CR's ^7Small Map Moshpit ^3(DLC)", 27018, 12, "nostalgia",
                "mp_shipment_s2:war", "mp_house:dom", "mp_gibraltar_02:conf", "mp_london:hp",
                "mp_france_village:dm", "mp_airship:war", "mp_fuhrerbunker:dom", "mp_v2_rocket_02:conf",
                "mp_monte_cassino_v2:hp", "mp_shipment_s2:conf", "mp_house:hp", "mp_gibraltar_02:dm",
                "mp_london:war", "mp_france_village:dom", "mp_airship:conf", "mp_fuhrerbunker:hp",
                "mp_v2_rocket_02:dm", "mp_monte_cassino_v2:war");
            fleet.Add(Pair(one, Running(one, 2, 14208, 6, 12, 24, TimeSpan.FromMinutes(192))));

            if (string.Equals(state, "one", StringComparison.OrdinalIgnoreCase)) return fleet;

            var two = Preset("^1CR's ^7TDM/KC Rotation", 27016, 8, "modern",
                "mp_aachen_v2:war", "mp_london:conf", "mp_d_day:war", "mp_gibraltar_02:conf",
                "mp_flak_tower:war", "mp_france_village:conf", "mp_carentan_s2:war", "mp_forest_01:conf");
            fleet.Add(Pair(two, Running(two, 4, 9931, 11, 7, 31, TimeSpan.FromMinutes(191))));

            var three = Preset("^4CR's ^7Dom/HP/TDM/KC ^49v9^7@Shipment", 27017, 17, "nostalgia",
                "mp_shipment_s2:dom", "mp_shipment_s2:hp", "mp_shipment_s2:war", "mp_shipment_s2:conf");

            if (string.Equals(state, "three-crashed", StringComparison.OrdinalIgnoreCase))
            {
                fleet.Add(Pair(three, new ServerState
                {
                    Port = three.Port,
                    Status = ServerStatus.Crashed,
                    Pid = 18422,
                    NoticedGone = DateTime.Now.AddMinutes(-2),
                    LastReply = DateTime.Now.AddMinutes(-2),
                    Cap = 18,
                }));
                return fleet;
            }

            // three-notanswering (and the plain three-server state)
            var stale = Running(three, 0, 18422, 0, 17, 0, TimeSpan.FromMinutes(189));
            stale.Status = ServerStatus.NotAnswering;
            stale.Misses = 3;
            stale.PingMs = 0;
            stale.LastReply = DateTime.Now.AddSeconds(-47);
            fleet.Add(Pair(three, stale));

            // One put away, so SHOW HIDDEN has something to count.
            var away = Preset("^2CR's ^7Gun Game", 27019, 10, "modern",
                "mp_shipment_s2:gun", "mp_house:gun", "mp_carentan_s2:gun");
            away.Hidden = true;
            fleet.Add(Pair(away, new ServerState { Port = away.Port }));
            return fleet;
        }

        /// <summary>The editor's three demo states: a filled multiplayer rotation, Zombies, empty.</summary>
        public static KeyValuePair<ServerPreset, ServerState> Editor(string state)
        {
            if (string.Equals(state, "zombies", StringComparison.OrdinalIgnoreCase))
            {
                var zombies = Preset("^5CR's ^7Zombies ^5/ ^7Four up", 27019, 0, "default",
                    "mp_zombie_descent:zombies", "mp_zombie_island:zombies",
                    "mp_zombie_berlin:zombies", "mp_zombie_nest_01:zombies");
                zombies.Mode = "zombies";
                zombies.MaxPlayers = 4;
                zombies.MinPlayers = 2;
                zombies.StartDelay = 45;
                return Pair(zombies, new ServerState { Port = zombies.Port });
            }

            if (string.Equals(state, "empty", StringComparison.OrdinalIgnoreCase))
            {
                var blank = Preset("^7New server", 27020, 12, "nostalgia");
                blank.FileName = "New server";
                return Pair(blank, new ServerState { Port = blank.Port });
            }

            var moshpit = Preset("^3CR's ^7Small Map Moshpit ^3(DLC)", 27018, 12, "nostalgia",
                "mp_shipment_s2:war", "mp_house:dom", "mp_gibraltar_02:conf", "mp_london:hp",
                "mp_france_village:dm", "mp_airship:war", "mp_fuhrerbunker:dom", "mp_v2_rocket_02:conf",
                "mp_monte_cassino_v2:hp", "mp_shipment_s2:conf", "mp_house:hp", "mp_gibraltar_02:dm");
            moshpit.BotDifficulty = "hardened";
            moshpit.MinPlayers = 2;
            moshpit.StartDelay = 45;
            moshpit.ScoreLimits["dom"] = 225;
            return Pair(moshpit, Running(moshpit, 2, 14208, 6, 12, 24, TimeSpan.FromMinutes(192)));
        }

        public static List<StarterViewModel> Starters(FleetViewModel fleet)
        {
            var bundled = PresetStore.Bundled();
            var starters = new List<StarterViewModel>();
            for (int i = 0; i < bundled.Count; i++)
            {
                var preset = bundled[i];
                starters.Add(new StarterViewModel
                {
                    Index = (i + 1).ToString("00"),
                    Title = preset.FileName,
                    Description = string.Join(", ", preset.Rotation.Select(r => GameData.MapName(r.Map)).Distinct().Take(4)) +
                                  ". " + string.Join(", ", preset.Rotation.Select(r => GameData.GametypeName(r.Gametype)).Distinct().Take(4)) + ".",
                    Meta = string.Format("{0} maps {1} {2} bots {1} :{3}",
                        preset.Rotation.Count, GameData.MiddleDot, preset.BotFill, preset.Port),
                    AddCommand = new RelayCommand(() => fleet.Toast("Demo mode: nothing was written")),
                });
            }
            return starters;
        }

        private static KeyValuePair<ServerPreset, ServerState> Pair(ServerPreset preset, ServerState state)
        {
            return new KeyValuePair<ServerPreset, ServerState>(preset, state);
        }

        private static ServerPreset Preset(string name, int port, int botFill, string botNames, params string[] rotation)
        {
            var preset = new ServerPreset
            {
                FileName = GameData.StripColorCodes(name),
                // A preset is its file here, so even the demo ones need a file of their own.
                FilePath = @"(demo)\server-" + port + ".json",
                ServerName = name,
                Port = port,
                BotFill = botFill,
                BotNames = botNames,
            };
            foreach (var pair in GameData.DefaultScoreLimits) preset.ScoreLimits[pair.Key] = pair.Value;
            foreach (var entry in rotation)
            {
                var parts = entry.Split(':');
                preset.Rotation.Add(new RotationEntry { Map = parts[0], Gametype = parts[1] });
            }
            return preset;
        }

        /// <summary>
        /// --screenshot-console has no server to read, so it writes a log of its own and points
        /// the drawer at that. The lines are the shape the fork writes: plain text, no
        /// timestamps, the server's own cfg echoed back at it as it is executed.
        /// </summary>
        public static List<string> ConsoleLog(ServerPreset preset)
        {
            var rotation = string.Join(" ", preset.Rotation.Take(6)
                .Select(r => "gametype " + r.Gametype + " map " + r.Map));
            var first = preset.Rotation.Count > 0 ? preset.Rotation[0] : new RotationEntry { Map = "mp_shipment_s2", Gametype = "war" };
            var second = preset.Rotation.Count > 1 ? preset.Rotation[1] : first;
            return new List<string>
            {
                "[S2x] dedicated server, build integration",
                "[S2x] net_port " + preset.Port + " · zombies 0 · noupdate",
                @"[FS] search paths: <game>\s2x, %LOCALAPPDATA%\s2x\data",
                "[FS] 4 fastfile search paths, 0 missing",
                "exec server-" + preset.Port + ".cfg",
                "set sv_hostname \"" + preset.PlainName + "\"",
                "set scr_war_scorelimit 75",
                "set scr_dom_scorelimit 200",
                "set scr_dom_halftime 0",
                "set scr_dom_roundlimit 1",
                "set bot_fill " + preset.BotFill,
                "set bot_names " + preset.BotNames,
                "set sv_maprotation \"" + rotation + " ...\"",
                "set bot_DifficultyDefault " + preset.BotDifficulty,
                "set party_maxplayers " + preset.MaxPlayers,
                "set party_minplayers " + preset.MinPlayers,
                "set party_matchStartDelay " + preset.StartDelay,
                "set master_server_enable 1",
                "set sv_lanOnly 0",
                "Loading zone common_mp",
                "Loading zone eng_common_mp",
                "Loading zone " + first.Map,
                "[SV] map_rotate → " + first.Map + " " + first.Gametype,
                "[SV] loading " + first.Map + " ... ok (2.4 s)",
                "[SV] dedicated party created, " + preset.MaxPlayers + " slots",
                "[Bots] added " + preset.BotFill + " bots (" + preset.BotNames + ")",
                "[Master] heartbeat sent to 49.12.1.118:20810",
                "[Master] listed · 1 of 1 heartbeat acknowledged",
                "[Query] reply 0/" + preset.MaxPlayers + " humans, " + preset.BotFill + " bots, 3 ms",
                "[SV] player connected: Ghost_Recon_88 (1 human)",
                "[Bots] removed 1 bot to make room",
                "[SV] player connected: xX_Sn1p3r_Xx (2 humans)",
                "[Chat] Ghost_Recon_88: gg",
                "[Query] reply 2/" + preset.MaxPlayers + " humans, " + (preset.BotFill - 1) + " bots, 24 ms",
                "[SV] warning: sv_maxclients changed while the lobby is up; it applies at the next map",
                "[SV] match ended · scorelimit",
                "[SV] map_rotate → " + second.Map + " " + second.Gametype,
                "Unloading zone " + first.Map,
                "Loading zone " + second.Map,
                "[SV] loading " + second.Map + " ... ok (1.9 s)",
                "[Bots] added 1 bot (" + preset.BotNames + ")",
                "[Query] reply 2/" + preset.MaxPlayers + " humans, " + preset.BotFill + " bots, 22 ms",
                "[Master] heartbeat sent to 49.12.1.118:20810",
            };
        }

        private static ServerState Running(ServerPreset preset, int mapIndex, int pid, int humans, int bots, int ping, TimeSpan uptime)
        {
            var entry = preset.Rotation[mapIndex % preset.Rotation.Count];
            return new ServerState
            {
                Port = preset.Port,
                Status = ServerStatus.Running,
                Pid = pid,
                ProcessStart = DateTime.Now - uptime,
                LastReply = DateTime.Now,
                Humans = humans,
                Bots = bots,
                Cap = preset.MaxPlayers,
                PingMs = ping,
                MapKey = entry.Map,
                GametypeKey = entry.Gametype,
                SvRunning = true,
            };
        }
    }
}
