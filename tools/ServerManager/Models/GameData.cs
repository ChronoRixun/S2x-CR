using System;
using System.Collections.Generic;
using System.Linq;

namespace S2x.ServerManager.Models
{
    /// <summary>
    /// The launcher's tables, same keys and same spellings as tools/server-launcher.ps1
    /// (lines 55-165) so both launchers name a map the same way.
    /// </summary>
    internal static class GameData
    {
        public const string MiddleDot = "·";

        public static readonly List<KeyValuePair<string, string>> Maps = Pairs(
            "mp_shipment_s2", "Shipment 1944",
            "mp_d_day", "Pointe du Hoc",
            "mp_aachen_v2", "Aachen",
            "mp_carentan_s2", "Carentan",
            "mp_carentan_s2_winter", "Winter Carentan",
            "mp_canon_farm", "Gustav Cannon",
            "mp_flak_tower", "Flak Tower",
            "mp_forest_01", "Ardennes Forest",
            "mp_london", "London Docks",
            "mp_france_village", "Sainte Marie du Mont",
            "mp_battleship_2", "USS Texas",
            "mp_gibraltar_02", "Gibraltar",
            "mp_sandbox_01", "Sandbox",
            "mp_house", "Groesten Haus",
            "mp_paris_s2", "Occupation",
            "mp_prague", "Anthropoid",
            "mp_wolfslair", "Valkyrie",
            "mp_dunkirk", "Dunkirk",
            "mp_egypt_02", "Egypt",
            "mp_v2_rocket_02", "V2",
            "mp_stalingrad", "Stalingrad",
            "mp_market_garden", "Market Garden",
            "mp_monte_cassino_v2", "Monte Cassino",
            "mp_tank_graveyard_2", "Excavation",
            "mp_airship", "Airship",
            "mp_fuhrerbunker", "Chancellery");

        // Maps that need a DLC pack on every player's client.
        public static readonly Dictionary<string, string> MapPacks = Map(
            "mp_carentan_s2", "Season Pass",
            "mp_carentan_s2_winter", "Season Pass",
            "mp_paris_s2", "DLC 1",
            "mp_prague", "DLC 1",
            "mp_wolfslair", "DLC 1",
            "mp_dunkirk", "DLC 2",
            "mp_egypt_02", "DLC 2",
            "mp_v2_rocket_02", "DLC 2",
            "mp_stalingrad", "DLC 3",
            "mp_market_garden", "DLC 3",
            "mp_monte_cassino_v2", "DLC 3",
            "mp_tank_graveyard_2", "DLC 4",
            "mp_airship", "DLC 4",
            "mp_fuhrerbunker", "DLC 4");

        public static readonly List<KeyValuePair<string, string>> ZombieMaps = Pairs(
            "mp_zombie_house", "Groesten Haus",
            "mp_zombie_nest_01", "The Final Reich",
            "mp_zombie_island", "The Darkest Shore",
            "mp_zombie_berlin", "The Shadowed Throne",
            "mp_zombie_windmill", "The Tortured Path: Into the Storm",
            "mp_zombie_dnk", "The Tortured Path: Across the Depths",
            "mp_zombie_dig_02", "The Tortured Path: Beyond the Veil",
            "mp_zombie_descent", "The Frozen Dawn");

        public static readonly Dictionary<string, string> ZombieMapPacks = Map(
            "mp_zombie_island", "DLC 1",
            "mp_zombie_berlin", "DLC 2",
            "mp_zombie_windmill", "DLC 3",
            "mp_zombie_dnk", "DLC 3",
            "mp_zombie_dig_02", "DLC 3",
            "mp_zombie_descent", "DLC 4");

        public static readonly List<KeyValuePair<string, string>> Gametypes = Pairs(
            "war", "Team Deathmatch",
            "dom", "Domination",
            "hp", "Hardpoint",
            "dm", "Free-for-All",
            "conf", "Kill Confirmed",
            "sd", "Search and Destroy",
            "ctf", "Capture the Flag",
            "gun", "Gun Game",
            "ball", "Gridiron");

        // Short labels for the card's NEXT line.
        public static readonly Dictionary<string, string> GametypeShortNames = Map(
            "war", "TDM",
            "dom", "DOM",
            "hp", "HP",
            "dm", "FFA",
            "conf", "KC",
            "sd", "S&D",
            "ctf", "CTF",
            "gun", "GUN",
            "ball", "GRID");

        public static readonly List<KeyValuePair<string, int>> DefaultScoreLimits = new List<KeyValuePair<string, int>>
        {
            new KeyValuePair<string, int>("war", 75),
            new KeyValuePair<string, int>("dom", 200),
            new KeyValuePair<string, int>("hp", 250),
            new KeyValuePair<string, int>("dm", 30),
            new KeyValuePair<string, int>("conf", 65),
            new KeyValuePair<string, int>("sd", 4),
            new KeyValuePair<string, int>("ctf", 3),
            new KeyValuePair<string, int>("gun", 18),
            new KeyValuePair<string, int>("ball", 28),
        };

        // Presets saved before v1.3.0 carry placeholder zone names the game never had.
        public static readonly Dictionary<string, string> LegacyZombieZones = Map(
            "nazi_zombie_proto", "mp_zombie_house",
            "nazi_zombie_asylum_f", "mp_zombie_descent",
            "nazi_zombie_island", "mp_zombie_island",
            "nazi_zombie_office", "mp_zombie_berlin",
            "nazi_zombie_treasure", "mp_zombie_windmill",
            "nazi_zombie_uss", "mp_zombie_dnk",
            "nazi_zombie_museum", "mp_zombie_dig_02",
            "nazi_zombie_mountaineer", "mp_zombie_nest_01");

        public static readonly string[] BotNamePools = { "default", "modern", "nostalgia" };

        // bot_DifficultyDefault takes these four.
        public static readonly string[] BotDifficulties = { "recruit", "regular", "hardened", "veteran" };

        /// <summary>The map table for the mode: Zombies has zones, multiplayer has maps.</summary>
        public static List<KeyValuePair<string, string>> MapsFor(bool zombies)
        {
            return zombies ? ZombieMaps : Maps;
        }

        // The engine renders ^0-^7 in sv_hostname; same mapping as Update-NamePreview.
        public static readonly Dictionary<char, string> ColorCodes = new Dictionary<char, string>
        {
            { '0', "#101010" }, { '1', "#E0332B" }, { '2', "#34C759" }, { '3', "#F5C400" },
            { '4', "#3C7BFF" }, { '5', "#2AD4E0" }, { '6', "#E040C8" }, { '7', "#F2F2F2" },
        };

        public static string MapName(string key)
        {
            if (string.IsNullOrEmpty(key)) return "—";
            foreach (var pair in Maps) if (pair.Key == key) return pair.Value;
            foreach (var pair in ZombieMaps) if (pair.Key == key) return pair.Value;
            return key;
        }

        public static string MapPack(string key)
        {
            string pack;
            if (key != null && MapPacks.TryGetValue(key, out pack)) return pack;
            if (key != null && ZombieMapPacks.TryGetValue(key, out pack)) return pack;
            return null;
        }

        public static string GametypeName(string key)
        {
            if (key == "zombies") return "Zombies";
            foreach (var pair in Gametypes) if (pair.Key == key) return pair.Value;
            return key ?? "";
        }

        public static string GametypeShort(string key)
        {
            string s;
            if (key != null && GametypeShortNames.TryGetValue(key, out s)) return s;
            return (key ?? "").ToUpperInvariant();
        }

        public static string StripColorCodes(string name)
        {
            if (string.IsNullOrEmpty(name)) return "";
            var sb = new System.Text.StringBuilder(name.Length);
            for (int i = 0; i < name.Length; i++)
            {
                if (name[i] == '^' && i + 1 < name.Length && name[i + 1] >= '0' && name[i + 1] <= '9') { i++; continue; }
                sb.Append(name[i]);
            }
            return sb.ToString();
        }

        private static List<KeyValuePair<string, string>> Pairs(params string[] flat)
        {
            var list = new List<KeyValuePair<string, string>>(flat.Length / 2);
            for (int i = 0; i + 1 < flat.Length; i += 2) list.Add(new KeyValuePair<string, string>(flat[i], flat[i + 1]));
            return list;
        }

        private static Dictionary<string, string> Map(params string[] flat)
        {
            return Pairs(flat).ToDictionary(p => p.Key, p => p.Value, StringComparer.Ordinal);
        }
    }
}
