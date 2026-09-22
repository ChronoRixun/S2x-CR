using System;
using System.Collections.Generic;
using System.Management;
using System.Text.RegularExpressions;

namespace S2x.ServerManager.Services
{
    internal sealed class S2xProcess
    {
        public int Pid;
        public int Port;
        public DateTime Started;
        public string CommandLine;
    }

    /// <summary>
    /// A pid file can outlive its server and Windows reuses pids, so a process counts as a
    /// port's server only when its command line has s2x.exe, -dedicated and net_port &lt;port&gt;.
    /// Same test as Stop-S2xServer in the PowerShell launcher, done here with WMI.
    /// </summary>
    internal static class ProcessInspector
    {
        private static readonly Regex PortArg = new Regex(@"net_port\s+(\d+)", RegexOptions.IgnoreCase);

        /// <summary>Every dedicated server running out of any folder, keyed by port.</summary>
        public static Dictionary<int, S2xProcess> DedicatedServers()
        {
            var found = new Dictionary<int, S2xProcess>();
            try
            {
                var query = new ObjectQuery(
                    "SELECT ProcessId, CommandLine, CreationDate FROM Win32_Process WHERE Name = 's2x.exe'");
                using (var searcher = new ManagementObjectSearcher(query))
                using (var results = searcher.Get())
                {
                    foreach (ManagementObject row in results)
                        using (row)
                        {
                            var commandLine = row["CommandLine"] as string;
                            if (!IsDedicated(commandLine)) continue;

                            var match = PortArg.Match(commandLine);
                            if (!match.Success) continue;

                            var server = new S2xProcess
                            {
                                Pid = Convert.ToInt32(row["ProcessId"]),
                                Port = int.Parse(match.Groups[1].Value),
                                CommandLine = commandLine,
                                Started = Started(row["CreationDate"] as string),
                            };
                            // Two on one port should not happen; the older one wins.
                            S2xProcess existing;
                            if (found.TryGetValue(server.Port, out existing) && existing.Started <= server.Started) continue;
                            found[server.Port] = server;
                        }
                }
            }
            catch
            {
                // WMI off or blocked: report nothing rather than guess, so nothing gets killed.
            }
            return found;
        }

        /// <summary>The safety check before Stop kills anything.</summary>
        public static bool IsServerFor(int pid, int port)
        {
            try
            {
                var query = new ObjectQuery("SELECT CommandLine FROM Win32_Process WHERE ProcessId = " + pid);
                using (var searcher = new ManagementObjectSearcher(query))
                using (var results = searcher.Get())
                {
                    foreach (ManagementObject row in results)
                        using (row)
                        {
                            var commandLine = row["CommandLine"] as string;
                            if (!IsDedicated(commandLine)) return false;
                            var match = PortArg.Match(commandLine);
                            return match.Success && match.Groups[1].Value == port.ToString();
                        }
                }
            }
            catch { }
            return false;
        }

        private static bool IsDedicated(string commandLine)
        {
            return !string.IsNullOrEmpty(commandLine)
                && commandLine.IndexOf("s2x.exe", StringComparison.OrdinalIgnoreCase) >= 0
                && commandLine.IndexOf("-dedicated", StringComparison.OrdinalIgnoreCase) >= 0;
        }

        private static DateTime Started(string wmiDate)
        {
            try { if (!string.IsNullOrEmpty(wmiDate)) return ManagementDateTimeConverter.ToDateTime(wmiDate); }
            catch { }
            return DateTime.Now;
        }
    }
}
