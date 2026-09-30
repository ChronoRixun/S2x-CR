using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;

namespace S2x.ServerManager.Services
{
    internal enum ScriptTargetState
    {
        Current,      // the copy there is the one this exe carries
        Installed,    // there was none, and now there is
        Updated,      // there was one this Manager wrote before, and it was replaced
        Custom,       // there is one nobody here wrote: it is the host's, and it is left alone
        Failed,       // it could not be read or written
    }

    internal sealed class ScriptTarget
    {
        public string RelativePath;
        public string FullPath;
        public ScriptTargetState State;
        public string Error;
    }

    internal sealed class ScriptInstallResult
    {
        public readonly List<ScriptTarget> Targets = new List<ScriptTarget>();

        public bool Failed { get { return Targets.Any(t => t.State == ScriptTargetState.Failed); } }
        public bool Custom { get { return Targets.Any(t => t.State == ScriptTargetState.Custom); } }

        /// <summary>What the host is told at launch, or null when every copy is this exe's own.</summary>
        public string Message
        {
            get
            {
                var failed = Targets.FirstOrDefault(t => t.State == ScriptTargetState.Failed);
                if (failed != null) return "could not write " + failed.RelativePath + ": " + failed.Error;
                var custom = Targets.FirstOrDefault(t => t.State == ScriptTargetState.Custom);
                if (custom != null) return "a custom copy of " + Path.GetFileName(custom.RelativePath) + " is in use";
                return null;
            }
        }
    }

    /// <summary>
    /// One server script this exe carries: its manifest resource (the csproj's LogicalName) and
    /// where it goes, relative to the game folder.
    /// </summary>
    internal sealed class EmbeddedScript
    {
        public EmbeddedScript(string resource, string[] targets)
        {
            Resource = resource;
            Targets = targets;
        }

        public string Resource { get; private set; }
        public string[] Targets { get; private set; }

        /// <summary>The bytes this exe carries.</summary>
        public byte[] Load()
        {
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream(Resource))
            {
                if (stream == null) throw new InvalidOperationException("This build does not carry " + Resource + ".");
                using (var copy = new MemoryStream())
                {
                    stream.CopyTo(copy);
                    return copy.ToArray();
                }
            }
        }
    }

    /// <summary>
    /// Puts the server scripts this exe carries into the game folder, so the release stays one
    /// file. A copy is written only where there is none, or where the one there is a copy a
    /// Manager wrote: one changed by hand, or brought from somewhere else, is the host's and is
    /// never written over. Nothing here deletes a script: other servers run from the same
    /// folder, and a script is inert until its dvar is set. Each script is installed on its own
    /// and only for a launch that uses it; the record of hashes is kept per target path, so
    /// scripts share it without touching each other's entries.
    /// </summary>
    internal sealed class ServerScriptInstaller
    {
        public const string AutoBalanceResource = "S2x.ServerManager.ServerScripts.s2x_autobalance.gsc";

        /// <summary>
        /// Where the auto-balance script goes, relative to the game folder. A copy per gametype
        /// folder (s2x\scripts\mp\war\ and so on) is more lines here and nothing else.
        /// </summary>
        public static readonly string[] AutoBalanceTargets =
        {
            @"s2x\scripts\mp\s2x_autobalance.gsc",
        };

        public const string ServerCmdsResource = "S2x.ServerManager.ServerScripts.s2x_servercmds.gsc";

        /// <summary>Where the chat commands and map vote script goes, relative to the game folder.</summary>
        public static readonly string[] ServerCmdsTargets =
        {
            @"s2x\scripts\mp\s2x_servercmds.gsc",
        };

        // Declared after the target lists: static fields are set in the order they are written.
        public static readonly EmbeddedScript AutoBalance = new EmbeddedScript(AutoBalanceResource, AutoBalanceTargets);
        public static readonly EmbeddedScript ServerCmds = new EmbeddedScript(ServerCmdsResource, ServerCmdsTargets);

        /// <summary>Every script this exe carries. A new one is an entry here and an EmbeddedResource in the csproj.</summary>
        public static readonly EmbeddedScript[] Catalog = { AutoBalance, ServerCmds };

        public const string RecordFileName = "installed-scripts.json";

        // A game server or a virus scanner can hold a script open for a moment: about a second
        // of retries in all.
        private const int Attempts = 5;
        private const int RetryMs = 100;

        // Two Starts at once would otherwise both find the file missing, and both rewrite the record.
        private static readonly object Gate = new object();

        public static string DefaultDirectory
        {
            get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "S2x", "ServerManager"); }
        }

        public ServerScriptInstaller(string directory = null)
        {
            RecordPath = Path.Combine(directory ?? DefaultDirectory, RecordFileName);
        }

        /// <summary>Every hash a Manager wrote to each target, so a later version knows what it may replace.</summary>
        public string RecordPath { get; private set; }

        public static byte[] AutoBalanceScript() { return AutoBalance.Load(); }

        public ScriptInstallResult InstallAutoBalance(string gameDir) { return InstallScript(gameDir, AutoBalance); }

        public static byte[] ServerCmdsScript() { return ServerCmds.Load(); }

        public ScriptInstallResult InstallServerCmds(string gameDir) { return InstallScript(gameDir, ServerCmds); }

        /// <summary>
        /// Makes every target of <paramref name="script"/> hold the copy this exe carries, unless the
        /// copy there is somebody else's. Not an overload of Install: the tests find methods by name.
        /// </summary>
        public ScriptInstallResult InstallScript(string gameDir, EmbeddedScript script)
        {
            return Install(gameDir, script.Load(), script.Targets);
        }

        /// <summary>Makes every target hold <paramref name="script"/>, unless the copy there is somebody else's.</summary>
        public ScriptInstallResult Install(string gameDir, byte[] script, IEnumerable<string> targets)
        {
            var result = new ScriptInstallResult();
            var hash = Hash(script);
            lock (Gate)
            {
                var record = ReadRecord();
                foreach (var relative in targets)
                {
                    var target = new ScriptTarget { RelativePath = relative, FullPath = Path.GetFullPath(Path.Combine(gameDir, relative)) };
                    result.Targets.Add(target);
                    Place(target, script, hash, record);
                }
            }
            return result;
        }

        private void Place(ScriptTarget target, byte[] script, string hash, Dictionary<string, List<string>> record)
        {
            for (int attempt = 1; ; attempt++)
            {
                try
                {
                    if (Directory.Exists(target.FullPath)) throw new IOException("a folder of that name is in the way");
                    var existing = File.Exists(target.FullPath) ? Hash(target.FullPath) : null;
                    if (existing == hash)
                    {
                        // Already this exe's copy. Taken as ours even when the record lost it,
                        // so the next version can still replace it.
                        Remember(record, target.FullPath, hash);
                        target.State = ScriptTargetState.Current;
                        return;
                    }
                    List<string> written;
                    if (existing != null && !(record.TryGetValue(target.FullPath, out written) && written.Contains(existing)))
                    {
                        target.State = ScriptTargetState.Custom;
                        return;
                    }

                    Directory.CreateDirectory(Path.GetDirectoryName(target.FullPath));
                    // Recorded before it is written: a copy of ours on disk is never one the
                    // record has not heard of.
                    Remember(record, target.FullPath, hash);
                    Write(target.FullPath, script);
                    target.State = existing == null ? ScriptTargetState.Installed : ScriptTargetState.Updated;
                    return;
                }
                catch (Exception ex)
                {
                    if (attempt < Attempts && Transient(ex)) { Thread.Sleep(RetryMs * attempt); continue; }
                    target.State = ScriptTargetState.Failed;
                    target.Error = ex.Message.Trim().TrimEnd('.');
                    return;
                }
            }
        }

        /// <summary>
        /// Writes beside the target and swaps it in, so a server starting at the same moment
        /// never reads half a script. The temporary name does not end in .gsc, because the
        /// engine compiles every .gsc it finds, and it never outlives this call.
        /// </summary>
        private static void Write(string path, byte[] script)
        {
            var temp = Path.Combine(Path.GetDirectoryName(path),
                Path.GetFileNameWithoutExtension(path) + "." + Guid.NewGuid().ToString("N") + ".writing");
            try
            {
                using (var stream = new FileStream(temp, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                {
                    stream.Write(script, 0, script.Length);
                    stream.Flush(true);
                }
                if (File.Exists(path)) File.Replace(temp, path, null);
                else File.Move(temp, path);     // throws when another writer got there first
            }
            finally
            {
                for (int attempt = 1; File.Exists(temp); attempt++)
                {
                    try { File.Delete(temp); }
                    catch (Exception) when (attempt < Attempts) { Thread.Sleep(RetryMs); }
                    catch (Exception) { break; }
                }
            }
        }

        /// <summary>
        /// Worth another go: a sharing or lock violation, a replace that could not move a file
        /// aside, or another writer that created the file first (the next look finds its copy).
        /// </summary>
        private static bool Transient(Exception ex)
        {
            if (!(ex is IOException) || ex is FileNotFoundException || ex is DirectoryNotFoundException) return false;
            switch (ex.HResult & 0xFFFF)
            {
                case 32:      // ERROR_SHARING_VIOLATION
                case 33:      // ERROR_LOCK_VIOLATION
                case 80:      // ERROR_FILE_EXISTS
                case 183:     // ERROR_ALREADY_EXISTS
                case 1175:    // ERROR_UNABLE_TO_REMOVE_REPLACED
                case 1176:    // ERROR_UNABLE_TO_MOVE_REPLACEMENT
                case 1177:    // ERROR_UNABLE_TO_MOVE_REPLACEMENT_2
                    return true;
                default:
                    return false;
            }
        }

        private static string Hash(byte[] bytes)
        {
            using (var sha = SHA256.Create()) return Hex(sha.ComputeHash(bytes));
        }

        private static string Hash(string path)
        {
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
            using (var sha = SHA256.Create())
                return Hex(sha.ComputeHash(stream));
        }

        private static string Hex(byte[] bytes)
        {
            var text = new StringBuilder(bytes.Length * 2);
            foreach (var b in bytes) text.Append(b.ToString("x2"));
            return text.ToString();
        }

        // ── the record ────────────────────────────────────────────────────────────
        /// <summary>
        /// Target path to every hash a Manager wrote there. A record that cannot be read is an
        /// empty one: every copy on disk then counts as the host's, which is the safe way round.
        /// </summary>
        private Dictionary<string, List<string>> ReadRecord()
        {
            var record = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
            try
            {
                if (!File.Exists(RecordPath)) return record;
                var root = new JavaScriptSerializer().DeserializeObject(File.ReadAllText(RecordPath)) as Dictionary<string, object>;
                if (root == null) return record;
                foreach (var pair in root)
                {
                    var hashes = pair.Value as object[];
                    if (hashes == null) continue;
                    record[pair.Key] = hashes.OfType<string>().Select(h => h.ToLowerInvariant()).Distinct().ToList();
                }
            }
            catch (Exception) { record.Clear(); }
            return record;
        }

        /// <summary>
        /// Adds the hash and writes the record back beside itself, swapped in the way the
        /// ownership store does. A record that cannot be written costs nothing now: the copy
        /// on disk still matches this exe, which is recognised the next time too.
        /// </summary>
        private void Remember(Dictionary<string, List<string>> record, string path, string hash)
        {
            List<string> hashes;
            if (!record.TryGetValue(path, out hashes)) record[path] = hashes = new List<string>();
            if (hashes.Contains(hash)) return;
            hashes.Add(hash);

            var temp = RecordPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(RecordPath));
                var text = new JavaScriptSerializer().Serialize(record.ToDictionary(p => p.Key, p => p.Value.ToArray()));
                File.WriteAllText(temp, text, new UTF8Encoding(false));
                if (File.Exists(RecordPath)) File.Replace(temp, RecordPath, null); else File.Move(temp, RecordPath);
            }
            catch (Exception) { }
            finally
            {
                try { if (File.Exists(temp)) File.Delete(temp); } catch (Exception) { }
            }
        }
    }
}
