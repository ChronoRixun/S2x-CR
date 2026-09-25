using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using S2x.ServerManager.Models;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// The launch profiles registered for a game folder: one package folder per line of
    /// &lt;game&gt;\s2x\launch-profiles.txt, each read from its own server-manager.json. Nothing
    /// here throws: a folder that cannot be read is a profile with a Problem and no entries.
    /// </summary>
    internal static class LaunchProfiles
    {
        public const string ProfileFile = "server-manager.json";

        public static string ListPath(string gameDir) { return Path.Combine(gameDir, @"s2x\launch-profiles.txt"); }

        /// <summary>The registered folders in file order. Blank lines and # comments are skipped.</summary>
        public static List<string> Folders(string gameDir)
        {
            try { return Lines(gameDir).Select(line => FolderOn(gameDir, line)).Where(f => f != null).ToList(); }
            catch { return new List<string>(); }
        }

        public static List<LaunchProfile> Load(string gameDir)
        {
            return Folders(gameDir).Select(Read).ToList();
        }

        public static void Add(string gameDir, string folder)
        {
            if (Folders(gameDir).Any(f => SameFolder(f, folder))) return;
            var lines = Lines(gameDir);
            lines.Add(Path.GetFullPath(folder));
            Write(gameDir, lines);
        }

        /// <summary>Drops the lines naming this folder. The rest stays as the host wrote it, comments included.</summary>
        public static void Remove(string gameDir, string folder)
        {
            Write(gameDir, Lines(gameDir).Where(line =>
            {
                var named = FolderOn(gameDir, line);
                return named == null || !SameFolder(named, folder);
            }).ToList());
        }

        private static List<string> Lines(string gameDir)
        {
            var path = ListPath(gameDir);
            return File.Exists(path) ? File.ReadAllLines(path).ToList() : new List<string>();
        }

        private static string FolderOn(string gameDir, string line)
        {
            line = line.Trim();
            if (line.Length == 0 || line.StartsWith("#", StringComparison.Ordinal)) return null;
            try { return Path.GetFullPath(Path.Combine(gameDir, line)); }
            catch { return null; }
        }

        private static void Write(string gameDir, List<string> lines)
        {
            var path = ListPath(gameDir);
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllLines(path, lines, new UTF8Encoding(false));
        }

        private static bool SameFolder(string a, string b)
        {
            return string.Equals(Path.GetFullPath(a).TrimEnd('\\'), Path.GetFullPath(b).TrimEnd('\\'),
                                 StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>The entry a preset names, and the profile it belongs to when that is registered.</summary>
        public static LaunchEntry Find(IEnumerable<LaunchProfile> profiles, string id, string key, out LaunchProfile profile)
        {
            profile = profiles == null || id == null ? null : profiles.FirstOrDefault(p => p.Id == id);
            return profile == null ? null : profile.Entries.FirstOrDefault(e => e.Key == key);
        }

        public static LaunchProfile Read(string folder)
        {
            var profile = new LaunchProfile { Folder = folder };
            try
            {
                var path = Path.Combine(folder, ProfileFile);
                if (!File.Exists(path)) { profile.Problem = ProfileFile + " not found"; return profile; }
                var root = new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path)) as Dictionary<string, object>;
                var entries = root == null ? null : Get(root, "entries") as object[];
                if (entries == null || Text(root, "profile") == null)
                {
                    profile.Problem = ProfileFile + " needs a profile name and a list of entries";
                    return profile;
                }

                var problems = new List<string>();
                foreach (var item in entries.OfType<Dictionary<string, object>>())
                {
                    var entry = new LaunchEntry
                    {
                        Key = Text(item, "key"),
                        Game = (Text(item, "game") ?? "zombies").ToLowerInvariant(),
                        Map = Text(item, "map"),
                        Mode = Text(item, "mode"),
                        Start = Text(item, "start"),
                        Public = Text(item, "public") ?? "",
                        Log = Text(item, "log"),
                    };
                    if (entry.Key == null || entry.Map == null || entry.Mode == null || entry.Start == null)
                    {
                        problems.Add("an entry without key, map, mode or start was skipped");
                        continue;
                    }
                    entry.Short = Text(item, "short") ?? entry.Mode;
                    entry.Script = ScriptOf(entry.Start);
                    if (!File.Exists(Path.Combine(folder, entry.Script)))
                    {
                        profile.MissingScripts[entry.Key] = entry.Script;
                        problems.Add("start script missing: " + entry.Script);
                        continue;
                    }
                    profile.Entries.Add(entry);
                }
                profile.Id = Text(root, "profile");
                profile.Title = Text(root, "title") ?? profile.Id;
                if (problems.Count > 0) profile.Problem = string.Join("; ", problems.Distinct());
            }
            catch (Exception ex)
            {
                // Half a profile is no profile: nothing from a file that failed partway is offered.
                profile.Entries.Clear();
                profile.Problem = ProfileFile + (ex is ArgumentException ? " is not valid JSON" : " could not be read: " + ex.Message);
            }
            return profile;
        }

        /// <summary>
        /// The script a start command runs: the argument after -File, or the command itself
        /// when it has no -File. Quotes group a path with spaces in it, as on a command line.
        /// </summary>
        public static string ScriptOf(string start)
        {
            var match = Regex.Match(start ?? "", @"(?:^|\s)-File\s+(""[^""]*""|\S+)", RegexOptions.IgnoreCase);
            if (!match.Success) match = Regex.Match(start ?? "", @"^\s*(""[^""]*""|\S+)");
            return match.Success ? match.Groups[1].Value.Trim('"') : "";
        }

        private static object Get(Dictionary<string, object> d, string key)
        {
            object value;
            return d.TryGetValue(key, out value) ? value : null;
        }

        private static string Text(Dictionary<string, object> d, string key)
        {
            var value = Get(d, key);
            var text = value == null ? null : Convert.ToString(value, CultureInfo.InvariantCulture).Trim();
            return string.IsNullOrEmpty(text) ? null : text;
        }
    }
}
