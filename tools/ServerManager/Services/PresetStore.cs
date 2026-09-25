using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Web.Script.Serialization;
using S2x.ServerManager.Models;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// Reads and writes the presets the PowerShell launcher keeps in &lt;game&gt;\s2x\presets.
    /// Names starting with "_" (_lastused, _lastused_mp, _lastused_zombies) are the launcher's
    /// own state; they are not servers, so they stay hidden.
    /// </summary>
    internal sealed class PresetStore
    {
        private readonly string _presetDir;

        public PresetStore(string gameDir, string presetDir = null)
        {
            _presetDir = string.IsNullOrEmpty(presetDir) ? GameFolder.PresetDir(gameDir) : Path.GetFullPath(presetDir);
        }

        public string Directory { get { return _presetDir; } }

        public List<ServerPreset> Load()
        {
            var presets = new List<ServerPreset>();
            if (!System.IO.Directory.Exists(_presetDir)) return presets;

            foreach (var file in System.IO.Directory.GetFiles(_presetDir, "*.json"))
            {
                var name = Path.GetFileNameWithoutExtension(file);
                if (name.StartsWith("_", StringComparison.Ordinal)) continue;
                var preset = Read(file);
                if (preset != null) presets.Add(preset);
            }
            return presets
                .OrderBy(p => p.Port)
                .ThenBy(p => p.FileName, StringComparer.CurrentCultureIgnoreCase)
                .ToList();
        }

        /// <summary>The presets bundled next to this exe, offered on the empty screen and in + New server.</summary>
        public static List<ServerPreset> Bundled()
        {
            var list = new List<ServerPreset>();
            var here = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            for (int up = 0; up < 6 && here != null; up++, here = Path.GetDirectoryName(here))
            {
                foreach (var folder in new[] { "presets", "server-presets" })
                {
                    var dir = Path.Combine(here, folder);
                    if (!System.IO.Directory.Exists(dir)) continue;
                    foreach (var file in System.IO.Directory.GetFiles(dir, "*.json"))
                    {
                        var preset = Read(file);
                        if (preset != null) list.Add(preset);
                    }
                    if (list.Count > 0) return list.OrderBy(p => p.FileName, StringComparer.CurrentCultureIgnoreCase).ToList();
                }
            }
            return list;
        }

        /// <summary>Copies a bundled preset in. A preset already under that name is left alone.</summary>
        public string Install(ServerPreset bundled)
        {
            System.IO.Directory.CreateDirectory(_presetDir);
            var target = Path.Combine(_presetDir, Path.GetFileName(bundled.FilePath));
            if (!File.Exists(target)) File.Copy(bundled.FilePath, target);
            return target;
        }

        public bool Exists(string name)
        {
            return File.Exists(Path.Combine(_presetDir, name + ".json"));
        }

        public string PathFor(string name) { return Path.Combine(_presetDir, name + ".json"); }

        /// <summary>One spelling of a preset's path, so a preset is one preset however it was named.</summary>
        public static string Key(string path)
        {
            if (string.IsNullOrEmpty(path)) return "";
            try { return Path.GetFullPath(path); }
            catch { return path; }
        }

        public static bool SamePath(string a, string b)
        {
            return string.Equals(Key(a), Key(b), StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>A name the launcher's preset box can also show, or the reason it cannot.</summary>
        public string NameProblem(string name, bool mustBeNew)
        {
            var problem = NameShape(name);
            if (problem != null) return problem;
            // Writing over another preset would lose it without asking, so a new name has to be new.
            if (mustBeNew && Exists(name.Trim())) return "There is already a preset called " + name.Trim() + ".";
            return null;
        }

        public static string NameShape(string name)
        {
            name = (name ?? "").Trim();
            if (name.Length == 0) return "A preset needs a name.";
            if (name.StartsWith("_", StringComparison.Ordinal))
                return "Names starting with _ belong to the launcher's own state.";
            if (name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
                return "A preset name cannot contain \\ / : * ? \" < > |.";
            return null;
        }

        /// <summary>The lowest free port at or above the launcher's default.</summary>
        public static int NextFreePort(IEnumerable<int> taken)
        {
            var used = new HashSet<int>(taken);
            for (int port = 27016; port <= 65535; port++) if (!used.Contains(port)) return port;
            return 27016;
        }

        /// <summary>Writes the preset. <paramref name="create"/> refuses to write over a file.</summary>
        public void Save(ServerPreset preset, bool create)
        {
            System.IO.Directory.CreateDirectory(_presetDir);
            if (string.IsNullOrEmpty(preset.FilePath)) preset.FilePath = PathFor(preset.FileName);
            PresetJson.Save(preset.FilePath, Compose(preset), create);
        }

        /// <summary>The preset as the file's keys, the launcher's spellings, unknown keys kept.</summary>
        private static Dictionary<string, object> Compose(ServerPreset preset)
        {
            var root = preset.Raw ?? new Dictionary<string, object>(StringComparer.Ordinal);
            preset.Raw = root;

            root["serverName"] = preset.ServerName;
            root["mode"] = preset.IsZombies ? "zombies" : "mp";
            // The PowerShell launcher does not know this key and starts such a preset as a plain
            // Zombies server, which is harmless.
            if (preset.IsProfile)
                root["launch"] = new Dictionary<string, object>(StringComparer.Ordinal)
                {
                    { "profile", preset.LaunchProfileId },
                    { "entry", preset.LaunchEntryKey },
                };
            else root.Remove("launch");
            root["rotation"] = preset.Rotation.Select(entry =>
            {
                // The entry the file had, with only the two keys this app owns updated, so a
                // key a newer launcher put on a line survives being reordered here.
                var item = entry.Raw ?? new Dictionary<string, object>(StringComparer.Ordinal);
                entry.Raw = item;
                item["gametype"] = entry.Gametype;
                item["map"] = entry.Map;
                return (object)item;
            }).ToArray();

            var scores = Get(root, "scoreLimits") as Dictionary<string, object>;
            if (scores == null) root["scoreLimits"] = scores = new Dictionary<string, object>(StringComparer.Ordinal);
            foreach (var pair in GameData.DefaultScoreLimits)
            {
                int value;
                scores[pair.Key] = preset.ScoreLimits.TryGetValue(pair.Key, out value) ? Clamp(value, 1, 999) : pair.Value;
            }

            root["singleRoundDom"] = preset.SingleRoundDom;
            root["botFill"] = preset.BotFill;
            root["botNames"] = preset.BotNames;
            root["port"] = preset.Port;

            root["botDifficulty"] = preset.BotDifficulty;
            root["maxPlayers"] = preset.MaxPlayers;
            root["minPlayers"] = preset.MinPlayers;
            root["startDelay"] = preset.StartDelay;
            root["advertise"] = preset.Advertise;
            root["extraLines"] = preset.ExtraLines.Cast<object>().ToArray();
            root["shuffleOnLaunch"] = preset.ShuffleOnLaunch;
            root["hidden"] = preset.Hidden;
            return root;
        }

        public static ServerPreset Read(string file)
        {
            try
            {
                var text = File.ReadAllText(file);
                var root = new JavaScriptSerializer().DeserializeObject(text) as Dictionary<string, object>;
                if (root == null) return null;

                var preset = new ServerPreset
                {
                    FilePath = file,
                    FileName = Path.GetFileNameWithoutExtension(file),
                    Raw = root,
                    ServerName = Str(root, "serverName") ?? "",
                    Mode = Str(root, "mode") ?? "mp",
                    BotNames = Str(root, "botNames") ?? "nostalgia",
                };
                preset.Port = Clamp(Int(root, "port", 27016), 1024, 65535);
                preset.BotFill = Clamp(Int(root, "botFill", 12), 0, 18);   // Set-Config clamps both ends
                preset.SingleRoundDom = Bool(root, "singleRoundDom", true);

                var ceiling = ServerPreset.CapCeiling(preset.IsZombies);
                preset.MaxPlayers = Clamp(Int(root, "maxPlayers", ceiling), 1, ceiling);
                preset.BotFill = Math.Min(preset.BotFill, preset.MaxPlayers);
                preset.MinPlayers = Clamp(Int(root, "minPlayers", 1), 1, preset.MaxPlayers);
                preset.StartDelay = Clamp(Int(root, "startDelay", 60), 0, ServerPreset.MaxStartDelay);
                preset.Advertise = Bool(root, "advertise", true);
                preset.ShuffleOnLaunch = Bool(root, "shuffleOnLaunch", false);
                preset.Hidden = Bool(root, "hidden", false);

                var launch = Get(root, "launch") as Dictionary<string, object>;
                if (launch != null)
                {
                    preset.LaunchProfileId = Str(launch, "profile");
                    preset.LaunchEntryKey = Str(launch, "entry");
                }

                var difficulty = (Str(root, "botDifficulty") ?? "regular").ToLowerInvariant();
                preset.BotDifficulty = Array.IndexOf(GameData.BotDifficulties, difficulty) >= 0 ? difficulty : "regular";

                var extra = Get(root, "extraLines") as object[];
                if (extra != null)
                    foreach (var line in extra)
                    {
                        var value = Convert.ToString(line, CultureInfo.InvariantCulture);
                        if (!string.IsNullOrWhiteSpace(value)) preset.ExtraLines.Add(value);
                    }

                var scores = Get(root, "scoreLimits") as Dictionary<string, object>;
                foreach (var pair in GameData.DefaultScoreLimits)
                {
                    var value = pair.Value;
                    if (scores != null) value = Int(scores, pair.Key, pair.Value);
                    preset.ScoreLimits[pair.Key] = Clamp(value, 1, 999);
                }

                var rotation = Get(root, "rotation") as object[];
                if (rotation != null)
                {
                    foreach (var item in rotation)
                    {
                        var entry = item as Dictionary<string, object>;
                        if (entry == null) continue;
                        var map = Str(entry, "map");
                        if (string.IsNullOrEmpty(map)) continue;
                        // Every restore path goes through here, so the legacy zone names the
                        // launcher used to list Zombies maps are rewritten the same way.
                        string current;
                        if (GameData.LegacyZombieZones.TryGetValue(map, out current)) map = current;
                        preset.Rotation.Add(new RotationEntry
                        {
                            Map = map,
                            Gametype = Str(entry, "gametype") ?? "war",
                            Raw = entry,
                        });
                    }
                }
                return preset;
            }
            catch
            {
                return null;
            }
        }

        private static object Get(Dictionary<string, object> d, string key)
        {
            object value;
            return d != null && d.TryGetValue(key, out value) ? value : null;
        }

        private static string Str(Dictionary<string, object> d, string key)
        {
            var value = Get(d, key);
            return value == null ? null : Convert.ToString(value, CultureInfo.InvariantCulture);
        }

        private static int Int(Dictionary<string, object> d, string key, int fallback)
        {
            var value = Get(d, key);
            int parsed;
            if (value == null) return fallback;
            if (value is int) return (int)value;
            return int.TryParse(Convert.ToString(value, CultureInfo.InvariantCulture), out parsed) ? parsed : fallback;
        }

        private static bool Bool(Dictionary<string, object> d, string key, bool fallback)
        {
            var value = Get(d, key);
            bool parsed;
            if (value is bool) return (bool)value;
            if (value == null) return fallback;
            return bool.TryParse(Convert.ToString(value, CultureInfo.InvariantCulture), out parsed) ? parsed : fallback;
        }

        private static int Clamp(int value, int low, int high)
        {
            return Math.Max(low, Math.Min(high, value));
        }
    }
}
