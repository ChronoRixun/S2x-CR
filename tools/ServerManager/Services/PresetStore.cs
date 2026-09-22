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
    /// Reads the presets the PowerShell launcher writes to &lt;game&gt;\s2x\presets. Names starting
    /// with "_" (_lastused, _lastused_mp, _lastused_zombies) are the launcher's own state; they
    /// are not servers, so they stay hidden.
    /// </summary>
    internal sealed class PresetStore
    {
        private readonly string _presetDir;

        public PresetStore(string gameDir)
        {
            _presetDir = GameFolder.PresetDir(gameDir);
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

        /// <summary>The presets bundled next to this exe, offered on the empty screen.</summary>
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
                        preset.Rotation.Add(new RotationEntry { Map = map, Gametype = Str(entry, "gametype") ?? "war" });
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
