using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using S2x.ServerManager.Models;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// Starts and stops the dedicated servers, writing the same files the PowerShell launcher
    /// writes so the two can share a game folder: s2x\server-&lt;port&gt;.cfg at launch, and
    /// s2x\server-&lt;port&gt;.pid for the process that owns the port.
    /// </summary>
    internal sealed class ServerController
    {
        private readonly string _gameDir;

        // Ports being started or stopped right now. Looking for a server on a port and then
        // launching one is two steps, and a double click or a Start racing Start All can slip
        // between them, so a port is claimed for the whole of it.
        private readonly HashSet<int> _busy = new HashSet<int>();

        public ServerController(string gameDir) { _gameDir = gameDir; }

        public string GameDir { get { return _gameDir; } }

        private bool Claim(int port)
        {
            lock (_busy) return _busy.Add(port);
        }

        private void Release(int port)
        {
            lock (_busy) _busy.Remove(port);
        }

        /// <summary>
        /// Writes the port's cfg from the preset, starts s2x.exe, then writes the pid file.
        /// The caller hands over a snapshot it will not touch again: an edit landing halfway
        /// through would otherwise write one port's cfg and start another one.
        /// </summary>
        public string Start(ServerPreset snapshot)
        {
            // One port for the claim, the checks, the files, the arguments and the release.
            var port = snapshot.Port;
            if (snapshot.Rotation.Count == 0) return "Add at least one map to the rotation first.";
            if (!File.Exists(GameFolder.ExePath(_gameDir))) return "s2x.exe is not in " + _gameDir + ".";
            if (!Claim(port)) return "Already working on :" + port + ".";

            try
            {
                // One server per port: never start a second one on a port that already has one,
                // and never guess when the process list could not be read.
                var scan = ProcessInspector.DedicatedServers();
                if (!scan.Complete)
                    return "Could not read the running servers, so :" + port + " cannot be checked. Nothing was started.";
                if (scan.Servers.ContainsKey(port))
                    return "A server is already running on :" + port + ".";

                var cfgPath = GameFolder.CfgPath(_gameDir, port);
                Directory.CreateDirectory(Path.GetDirectoryName(cfgPath));
                File.WriteAllText(cfgPath, BuildServerCfg(snapshot), new UTF8Encoding(false));

                // Both modes start from the rotation the port's cfg carries; a command-line +map
                // runs before the dedicated party exists and is dropped. g_consoleLog gives this
                // server a log of its own: the default is s2x\logs\console.log, which every
                // server on the box appends to at once, so nothing in it says which one wrote it.
                var args = string.Format(
                    "-noupdate -dedicated{0} +set net_port {1} +set g_consoleLog {2} +exec {3} +map_rotate",
                    snapshot.IsZombies ? " -zombies" : "", port,
                    GameFolder.ServerLogDvar(port), Path.GetFileName(cfgPath));

                var process = Process.Start(new ProcessStartInfo
                {
                    FileName = GameFolder.ExePath(_gameDir),
                    Arguments = args,
                    WorkingDirectory = _gameDir,
                    UseShellExecute = false,
                });
                if (process == null) return "Windows did not start s2x.exe.";
                File.WriteAllText(GameFolder.PidPath(_gameDir, port), process.Id.ToString());
                return null;
            }
            catch (Exception ex)
            {
                // A locked s2x folder or a full disk is the host's problem to read, not a crash:
                // Start All has to carry on down the list.
                return ex.Message;
            }
            finally
            {
                Release(port);
            }
        }

        /// <summary>Terminates only the verified server on this port, then drops the pid file.</summary>
        public string Stop(int port)
        {
            if (!Claim(port)) return "Already working on :" + port + ".";

            try
            {
                var pids = new List<int>();

                S2xProcess running;
                if (ProcessInspector.DedicatedServers().Servers.TryGetValue(port, out running)) pids.Add(running.Pid);

                var pidPath = GameFolder.PidPath(_gameDir, port);
                var saved = ReadPid(pidPath);
                if (saved > 0 && !pids.Contains(saved)) pids.Add(saved);

                string failure = null;
                foreach (var pid in pids)
                {
                    // A pid file can outlive its server and Windows reuses pids, so check the
                    // command line first: never a game client, never somebody else's process.
                    if (!ProcessInspector.IsServerFor(pid, port)) continue;
                    try { Process.GetProcessById(pid).Kill(); }
                    catch (Exception ex) { failure = ex.Message; }
                }

                try { if (File.Exists(pidPath)) File.Delete(pidPath); }
                catch (Exception ex) { failure = failure ?? ex.Message; }
                return failure;
            }
            catch (Exception ex)
            {
                return ex.Message;
            }
            finally
            {
                Release(port);
            }
        }

        /// <summary>Every server that is not up, in port order, two seconds apart.</summary>
        public async Task<string> StartAllAsync(IEnumerable<ServerPreset> presets, Action<string> onFailure)
        {
            var queue = presets.OrderBy(p => p.Port).ToList();
            for (int i = 0; i < queue.Count; i++)
            {
                if (i > 0) await Task.Delay(2000).ConfigureAwait(true);
                var failure = Start(queue[i]);
                if (failure != null && onFailure != null) onFailure(failure);
            }
            return null;
        }

        public static int ReadPid(string pidPath)
        {
            try
            {
                if (!File.Exists(pidPath)) return 0;
                var match = System.Text.RegularExpressions.Regex.Match(File.ReadAllText(pidPath), @"^\s*(\d+)");
                return match.Success ? int.Parse(match.Groups[1].Value) : 0;
            }
            catch { return 0; }
        }

        private static readonly Random Shuffler = new Random();

        /// <summary>
        /// Build-ServerCfg's lines in its order first, so a preset the PowerShell launcher wrote
        /// still comes out as the text it wrote, then the editor's own lines, then the host's
        /// advanced block last so it wins. Bots and score limits are multiplayer only.
        /// </summary>
        public static string BuildServerCfg(ServerPreset preset)
        {
            var zombies = preset.IsZombies;
            // The hostname is quoted in the cfg, so a quote or a newline in it would end the
            // line early and feed the rest to the console. Colour codes stay.
            var hostname = new string((preset.ServerName ?? "")
                .Where(c => c != '"' && c != '\r' && c != '\n').ToArray());
            var lines = new List<string> { "set sv_hostname \"" + hostname + "\"" };

            if (!zombies)
            {
                var used = new HashSet<string>(preset.Rotation.Select(r => r.Gametype));
                foreach (var pair in GameData.DefaultScoreLimits)
                {
                    if (!used.Contains(pair.Key) && preset.Rotation.Count != 0) continue;
                    int value;
                    if (!preset.ScoreLimits.TryGetValue(pair.Key, out value)) value = pair.Value;
                    lines.Add("set scr_" + pair.Key + "_scorelimit " + Math.Max(1, Math.Min(999, value)));
                }
            }

            if (preset.SingleRoundDom)
            {
                lines.Add("set scr_dom_halftime 0");
                lines.Add("set scr_dom_roundlimit 1");
            }

            if (!zombies)
            {
                lines.Add("set bot_fill " + preset.BotFill);
                lines.Add("set bot_names " + preset.BotNames);
            }

            if (preset.Rotation.Count > 0)
            {
                // Shuffle on every launch shuffles what the cfg says, not the preset: the host
                // keeps the order they typed.
                var rotation = preset.Rotation.ToList();
                if (preset.ShuffleOnLaunch)
                    for (int i = rotation.Count - 1; i > 0; i--)
                    {
                        var j = Shuffler.Next(i + 1);
                        var hold = rotation[i]; rotation[i] = rotation[j]; rotation[j] = hold;
                    }
                var parts = rotation.Select(r => "gametype " + r.Gametype + " map " + r.Map);
                lines.Add("set sv_maprotation \"" + string.Join(" ", parts) + "\"");
            }

            lines.Add("set bot_DifficultyDefault " + preset.BotDifficulty);
            lines.Add("set party_maxplayers " + preset.MaxPlayers);
            lines.Add("set party_minplayers " + preset.MinPlayers);
            lines.Add("set party_matchStartDelay " + preset.StartDelay);
            lines.Add("set master_server_enable " + (preset.Advertise ? "1" : "0"));
            lines.Add("set sv_lanOnly " + (preset.Advertise ? "0" : "1"));

            // Passed through as written: a blank line is dropped, anything else goes as typed.
            foreach (var line in preset.ExtraLines)
                if (!string.IsNullOrWhiteSpace(line)) lines.Add(line);

            return string.Join("\n", lines);
        }
    }
}
