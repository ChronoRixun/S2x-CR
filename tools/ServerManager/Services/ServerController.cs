using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
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
        private readonly ManagedServerOwnershipStore _adminOwnership;
        private readonly ServerScriptInstaller _scripts;

        // Ports being started or stopped right now. Looking for a server on a port and then
        // launching one is two steps, and a double click or a Start racing Start All can slip
        // between them, so a port is claimed for the whole of it.
        private readonly HashSet<int> _busy = new HashSet<int>();

        // A profile's start script stages its runtime and checks its hashes before it launches,
        // then the server it started has to turn up in the process list.
        private const int ProfileScriptSeconds = 90;
        private const int ProfileServerSeconds = 10;

        public ServerController(string gameDir) : this(gameDir, new ManagedServerOwnershipStore()) { }
        public ServerController(string gameDir, ManagedServerOwnershipStore ownership, ServerScriptInstaller scripts = null)
        {
            _gameDir = gameDir;
            _adminOwnership = ownership ?? throw new ArgumentNullException(nameof(ownership));
            _scripts = scripts ?? new ServerScriptInstaller();
        }

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

                if (snapshot.IsProfile) return StartProfile(snapshot, port);

                // The scripts go into the game folder before the cfg that switches them on.
                var note = PrepareScripts(snapshot);

                var cfgPath = GameFolder.CfgPath(_gameDir, port);
                Directory.CreateDirectory(Path.GetDirectoryName(cfgPath));
                File.WriteAllText(cfgPath, BuildServerCfg(snapshot), new UTF8Encoding(false));

                // Both modes start from the rotation the port's cfg carries; a command-line +map
                // runs before the dedicated party exists and is dropped. g_consoleLog gives this
                // server a log of its own: the default is s2x\logs\console.log, which every
                // server on the box appends to at once, so nothing in it says which one wrote it.
                // Supplying zombiesMode below avoids the bootstrap replacing the owned process.
                var adminNonce = Guid.NewGuid().ToString("N");
                var args = "-server-manager-admin " + adminNonce + " " + string.Format(
                    "-noupdate -dedicated{0} +set net_port {1} +set sv_lanOnly {4} +set master_server_enable {5} +set g_consoleLog {2} +exec {3} +map_rotate",
                    snapshot.IsZombies ? " -zombies +zombiesMode 1" : "", port,
                    GameFolder.ServerLogDvar(port), Path.GetFileName(cfgPath), snapshot.Advertise ? 0 : 1, snapshot.Advertise ? 1 : 0);

                var process = Process.Start(new ProcessStartInfo
                {
                    FileName = GameFolder.ExePath(_gameDir),
                    Arguments = args,
                    WorkingDirectory = _gameDir,
                    UseShellExecute = false,
                });
                if (process == null) return "Windows did not start s2x.exe.";
                File.WriteAllText(GameFolder.PidPath(_gameDir, port), process.Id.ToString());
                try { _adminOwnership.Register(process.Id, port, adminNonce); }
                catch (Exception ex) { return "Server started, but administration ownership could not be saved: " + ex.Message + (note == null ? "" : " " + note); }
                // It started; the note goes into the toast that says so.
                return note == null ? null : "Starting " + snapshot.PlainName + " on :" + port + ". " + note;
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

        /// <summary>
        /// Every server script this launch uses, in place before its cfg is written, and what
        /// the host should be told about them as one note for the start toast, or null. Each
        /// script stands on its own: one that cannot be put in place turns off only what it runs.
        /// </summary>
        public string PrepareScripts(ServerPreset snapshot)
        {
            var notes = new[] { PrepareAutoBalance(snapshot), PrepareServerCmds(snapshot) }
                .Where(note => note != null).ToList();
            return notes.Count == 0 ? null : string.Join(" ", notes);
        }

        /// <summary>
        /// Puts s2x_autobalance.gsc into the game folder for a launch that uses it, and returns
        /// what the host should be told, or null. A script that could not be put in place turns
        /// auto-balance off for this launch only: the snapshot is the launch's own copy, so the
        /// cfg goes back to the native bot fill and the server still gets its bots.
        /// </summary>
        public string PrepareAutoBalance(ServerPreset snapshot)
        {
            if (!snapshot.UsesAutoBalance) return null;
            string problem;
            try
            {
                var result = _scripts.InstallAutoBalance(_gameDir);
                if (!result.Failed) return result.Custom ? Capital(result.Message) + "." : null;
                problem = result.Message;
            }
            catch (Exception ex)
            {
                problem = ex.Message.Trim().TrimEnd('.');
            }
            snapshot.AutoBalance = false;
            return "Auto-balance is not active (" + problem + "); bots use the normal bot fill.";
        }

        /// <summary>
        /// Puts s2x_servercmds.gsc into the game folder for a launch that uses the chat commands
        /// or the map vote, and returns what the host should be told, or null. A script that
        /// could not be put in place turns both off for this launch only (the snapshot is the
        /// launch's own copy): the cfg then says 0 for them, and the server starts without them.
        /// </summary>
        public string PrepareServerCmds(ServerPreset snapshot)
        {
            if (!snapshot.UsesServerCmds) return null;
            string problem;
            try
            {
                var result = _scripts.InstallServerCmds(_gameDir);
                if (!result.Failed) return result.Custom ? Capital(result.Message) + "." : null;
                problem = result.Message;
            }
            catch (Exception ex)
            {
                problem = ex.Message.Trim().TrimEnd('.');
            }
            // Only what was on is named, so the note never claims more than this server had.
            var what = snapshot.UsesChatCommands && snapshot.UsesMapVote ? "Chat commands and the map vote are"
                : snapshot.UsesChatCommands ? "Chat commands are" : "The map vote is";
            snapshot.ChatCommands = false;
            snapshot.MapVote = false;
            return what + " not active (" + problem + ").";
        }

        private static string Capital(string text)
        {
            return string.IsNullOrEmpty(text) ? text : char.ToUpperInvariant(text[0]) + text.Substring(1);
        }

        /// <summary>
        /// A launch profile's server: the package's own script stages and starts it, and this
        /// finds what it started the way Stop will, by -dedicated and net_port on the command
        /// line. The pid file is written as for any other server, so Stop and the fleet treat
        /// it the same.
        /// </summary>
        private string StartProfile(ServerPreset snapshot, int port)
        {
            LaunchProfile profile;
            var entry = LaunchProfiles.Find(LaunchProfiles.Load(_gameDir), snapshot.LaunchProfileId, snapshot.LaunchEntryKey, out profile);
            if (profile == null) return "The profile '" + snapshot.LaunchProfileId + "' is not registered.";
            if (entry == null)
            {
                string script;
                return profile.MissingScripts.TryGetValue(snapshot.LaunchEntryKey, out script)
                    ? "The profile '" + profile.Title + "' is missing " + script + "."
                    : "The profile '" + profile.Title + "' has no entry " + snapshot.LaunchEntryKey + ".";
            }

            string program, arguments;
            var adminNonce = string.IsNullOrWhiteSpace(entry.Admin) ? null : Guid.NewGuid().ToString("N");
            var command = FillCommand(entry, port, snapshot);
            if (adminNonce != null) command += " " + entry.Admin.Replace("{admin}", adminNonce);
            SplitCommand(command, out program, out arguments);
            var output = new List<string>();
            var errors = new List<string>();
            var closed = new CountdownEvent(2);
            // Not disposed: a server the script started can hold the pipes open after the script
            // is gone, and a read still pending on a disposed process would take the app down.
            var process = new Process
            {
                StartInfo = new ProcessStartInfo
                {
                    FileName = program,
                    Arguments = arguments,
                    WorkingDirectory = profile.Folder,
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                },
            };
            process.OutputDataReceived += (s, e) => Collect(output, e.Data, closed);
            process.ErrorDataReceived += (s, e) => Collect(errors, e.Data, closed);
            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            if (!process.WaitForExit(ProfileScriptSeconds * 1000))
            {
                try { process.Kill(); } catch { }
                closed.Wait(5000);
                return "The start script did not finish in " + ProfileScriptSeconds + " s." + Said(output, errors);
            }
            closed.Wait(5000);
            if (process.ExitCode != 0)
            {
                var said = Said(output, errors);
                return said.Length > 0 ? said.Trim() : "The start script failed with exit code " + process.ExitCode + ".";
            }

            for (int second = 0; ; second++)
            {
                S2xProcess server;
                if (ProcessInspector.DedicatedServers().Servers.TryGetValue(port, out server) &&
                    (adminNonce == null || ManagedServerOwnershipStore.HasNonce(server.CommandLine, adminNonce)))
                {
                    File.WriteAllText(GameFolder.PidPath(_gameDir, port), server.Pid.ToString());
                    if (adminNonce != null)
                    {
                        try { _adminOwnership.Register(server.Pid, port, adminNonce); }
                        catch (Exception ex) { return "Server started, but administration ownership could not be saved: " + ex.Message; }
                    }
                    return null;
                }
                if (second == ProfileServerSeconds) break;
                Thread.Sleep(1000);
            }
            return "The start script finished, but no " + (adminNonce == null ? "server" : "server matching this Manager launch") + " came up on :" + port + "." + Said(output, errors);
        }

        /// <summary>The entry's start command for this server, its placeholders filled.</summary>
        private string FillCommand(LaunchEntry entry, int port, ServerPreset snapshot)
        {
            // The name and the folder land between quotes: a quote or a line break would end the
            // argument early, and a backslash in front of the closing quote would escape it.
            var name = new string((snapshot.ServerName ?? "").Where(c => c != '"' && c != '\r' && c != '\n').ToArray()).TrimEnd('\\');
            var text = entry.Start
                .Replace("{gameDir}", _gameDir.TrimEnd('\\'))
                .Replace("{port}", port.ToString())
                .Replace("{name}", name);
            // Nothing to put in means the space in front of it goes too.
            return snapshot.Advertise && entry.Public.Length > 0
                ? text.Replace("{public}", entry.Public)
                : text.Replace(" {public}", "").Replace("{public}", "");
        }

        /// <summary>The first token is the program; the rest of the line is its arguments as written.</summary>
        private static void SplitCommand(string command, out string program, out string arguments)
        {
            var match = Regex.Match(command.Trim(), @"^(""[^""]*""|\S+)\s*(.*)$", RegexOptions.Singleline);
            program = match.Groups[1].Value.Trim('"');
            arguments = match.Groups[2].Value;
        }

        private static void Collect(List<string> lines, string line, CountdownEvent closed)
        {
            if (line == null) { closed.Signal(); return; }
            lock (lines) if (line.Trim().Length > 0) lines.Add(line.Trim());
        }

        /// <summary>
        /// What the script said, for the toast: the message of the error it stopped on, or the
        /// last three lines it wrote. PowerShell writes an uncaught error as its message, then
        /// where it was raised ("At &lt;script&gt;:&lt;line&gt;") and a picture of that line.
        /// </summary>
        private static string Said(List<string> output, List<string> errors)
        {
            List<string> lines;
            lock (errors) lines = errors.TakeWhile(line => !line.StartsWith("At ", StringComparison.Ordinal)).ToList();
            if (lines.Count == 0) lock (output) lines = output.ToList();
            if (lines.Count == 0) return "";
            return " " + string.Join(" " + GameData.MiddleDot + " ", lines.Skip(Math.Max(0, lines.Count - 3)));
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
                // Off the window's thread, as a single Start is: a profile's script can take
                // most of a minute, and the window would hang for all of it.
                var preset = queue[i];
                var failure = await Task.Run(() => Start(preset)).ConfigureAwait(true);
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
        /// advanced block last so it wins. Bots, auto-balance, score limits, chat commands and the
        /// map vote are multiplayer only.
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

            // With auto-balance the script owns every bot, so the native one-shot fill adds none.
            var autoBalance = preset.UsesAutoBalance;
            if (!zombies)
            {
                lines.Add("set bot_fill " + (autoBalance ? 0 : preset.BotFill));
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
            // The limits were party_maxplayers and party_minplayers, which the game swallows
            // before the server sees them, so no exe ever applied them (S2x-CR#39). v1.6.1
            // reads these names.
            lines.Add("set sv_maxplayers " + preset.MaxPlayers);
            lines.Add("set sv_minplayers " + preset.MinPlayers);
            lines.Add("set party_matchStartDelay " + preset.StartDelay);
            lines.Add("set master_server_enable " + (preset.Advertise ? "1" : "0"));
            lines.Add("set sv_lanOnly " + (preset.Advertise ? "0" : "1"));

            // s2x_autobalance.gsc keeps the match at the bot fill size and the teams even; the
            // game's own team balance would move players under it. Off is written too: every
            // multiplayer server in this game folder loads the script, and this keeps it inert
            // on the ones that do not use it.
            if (autoBalance)
            {
                lines.Add("set s2x_autobalance 1");
                lines.Add("set s2x_autobalance_target " + preset.BotFill);
                lines.Add("set scr_teambalance 0");
            }
            else if (!zombies)
            {
                lines.Add("set s2x_autobalance 0");
            }

            // s2x_servercmds.gsc: chat commands and the end-of-match map vote. Off is written
            // too, for the reason auto-balance's is: every multiplayer server in the folder loads
            // the script. The host's text is cleaned for the cfg here, never when it is saved.
            if (!zombies) lines.AddRange(ServerCmdsLines(preset));

            // Passed through as written: a blank line is dropped, anything else goes as typed.
            foreach (var line in preset.ExtraLines)
                if (!string.IsNullOrWhiteSpace(line)) lines.Add(line);

            return string.Join("\n", lines);
        }

        /// <summary>
        /// The chat commands' and the map vote's lines, each off line always there. Rules are
        /// numbered 1..k in order, a rule with nothing left once cleaned taking no number, since
        /// the script stops at the first one that is empty.
        /// </summary>
        private static IEnumerable<string> ServerCmdsLines(ServerPreset preset)
        {
            var chat = preset.UsesChatCommands;
            yield return "set s2x_chatcmds " + (chat ? 1 : 0);
            if (chat)
            {
                var number = 0;
                foreach (var rule in preset.Rules)
                {
                    var text = CfgText.Clean(rule, CfgText.RuleBytes);
                    if (text.Length == 0) continue;
                    if (++number > ServerPreset.MaxRules) break;
                    yield return "set s2x_rules" + number + " \"" + text + "\"";
                }
                var discord = CfgText.Clean(preset.Discord, CfgText.DiscordBytes);
                if (discord.Length > 0) yield return "set s2x_discord \"" + discord + "\"";
            }

            var vote = preset.UsesMapVote;
            yield return "set s2x_mapvote " + (vote ? 1 : 0);
            if (vote)
            {
                yield return "set s2x_mapvote_choices " + Math.Max(ServerPreset.MinVoteChoices, Math.Min(ServerPreset.MaxVoteChoices, preset.VoteChoices));
                yield return "set s2x_mapvote_time " + Math.Max(ServerPreset.MinVoteSeconds, Math.Min(ServerPreset.MaxVoteSeconds, preset.VoteSeconds));
            }
        }
    }
}
