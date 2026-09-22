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
            return fleet;
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
                FilePath = "(demo)",
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
                Cap = preset.PlayerCap,
                PingMs = ping,
                MapKey = entry.Map,
                GametypeKey = entry.Gametype,
                SvRunning = true,
            };
        }
    }
}
