using System;
using System.Diagnostics;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Threading;
using Microsoft.Win32.SafeHandles;
using System.IO;
using System.Management;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace S2x.ServerManager.Services
{
    public sealed class ManagedServerRecord
    {
        public int Pid { get; set; }
        public long CreatedUtcTicks { get; set; }
        public string ExePath { get; set; }
        public int Port { get; set; }
        public string Instance { get; set; }
    }

    public sealed class ManagedServerOwnershipStore
    {
        public static string DefaultDirectory => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "S2x", "ServerManager", "managed-servers");
        public string DirectoryPath { get; }
        public ManagedServerOwnershipStore(string directory = null) { DirectoryPath = directory ?? DefaultDirectory; }
        public static bool HasNonce(string commandLine, string nonce)
        {
            return nonce != null && Regex.IsMatch(nonce, "^[0-9a-f]{32}$") &&
                Regex.Matches(commandLine ?? "", @"(?:^|\s)-server-manager-admin\s+([0-9a-f]{32})(?=\s|$)").Count == 1 &&
                Regex.IsMatch(commandLine ?? "", @"(?:^|\s)-server-manager-admin\s+" + nonce + @"(?=\s|$)");
        }
        public ManagedServerRecord Read(int port)
        {
            var path = Path.Combine(DirectoryPath, port + ".json");
            if (!File.Exists(path)) return null;
            var record = new JavaScriptSerializer().Deserialize<ManagedServerRecord>(File.ReadAllText(path));
            if (record == null || record.Port != port || !Regex.IsMatch(record.Instance ?? "", "^[0-9a-f]{32}$")) throw new InvalidDataException("Invalid Manager ownership record.");
            return record;
        }
        // Called only after Start created a process carrying this launch's fresh nonce.
        public void Register(int pid, int port, string nonce)
        {
            using (var process = Process.GetProcessById(pid))
            {
                // Pin this process identity once. Retries must never adopt a replacement PID.
                var record = new ManagedServerRecord { Pid = pid, Port = port, Instance = nonce,
                    CreatedUtcTicks = process.StartTime.ToUniversalTime().Ticks };
                var waiting = Stopwatch.StartNew();
                while (true)
                {
                    if (process.HasExited) throw new InvalidOperationException("The Manager-started server exited before ownership could be recorded.");
                    try
                    {
                        record.ExePath = Path.GetFullPath(ExecutablePath(process.Id));
                        Validate(record);
                        break;
                    }
                    catch (Exception ex) when (ex is StartupIdentityUnavailableException || ex is Win32Exception || ex is ManagementException)
                    {
                        var remaining = 2000 - (int)waiting.ElapsedMilliseconds;
                        if (remaining <= 0) throw;
                        Thread.Sleep(Math.Min(100, remaining));
                    }
                }
                Directory.CreateDirectory(DirectoryPath);
                var path = Path.Combine(DirectoryPath, port + ".json");
                var temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
                try
                {
                    File.WriteAllText(temp, new JavaScriptSerializer().Serialize(record), new UTF8Encoding(false));
                    if (File.Exists(path)) File.Replace(temp, path, null); else File.Move(temp, path);
                }
                finally { if (File.Exists(temp)) File.Delete(temp); }
            }
        }
        private sealed class StartupIdentityUnavailableException : InvalidOperationException
        {
            public StartupIdentityUnavailableException(string message) : base(message) { }
        }
        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern SafeProcessHandle OpenProcess(uint access, bool inherit, int pid);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool QueryFullProcessImageNameW(SafeProcessHandle process, uint flags, StringBuilder path, ref int size);
        private static string ExecutablePath(int pid)
        {
            // WMI ExecutablePath can be null during startup and across bitness boundaries.
            using (var handle = OpenProcess(0x1000 /* PROCESS_QUERY_LIMITED_INFORMATION */, false, pid))
            {
                if (handle.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot open the server process for identity verification.");
                var path = new StringBuilder(32768);
                var size = path.Capacity;
                if (!QueryFullProcessImageNameW(handle, 0, path, ref size))
                    throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot verify the server executable path.");
                return path.ToString();
            }
        }
        public void Validate(ManagedServerRecord record)
        {
            if (record == null || record.Port < 1 || record.Port > 65535 || record.Pid <= 0 || !Regex.IsMatch(record.Instance ?? "", "^[0-9a-f]{32}$"))
                throw new InvalidOperationException("Administration is unavailable: no compatible Manager administration launch is recorded. Relaunch with a compatible server build or profile.");
            Process process;
            try { process = Process.GetProcessById(record.Pid); }
            catch (ArgumentException) { throw new InvalidOperationException("The server this Manager started on :" + record.Port + " is no longer running."); }
            using (process)
            {
                if (process.StartTime.ToUniversalTime().Ticks != record.CreatedUtcTicks ||
                    !string.Equals(Path.GetFullPath(ExecutablePath(process.Id)), record.ExePath, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("The saved server process identity no longer matches.");
            }
            using (var search = new ManagementObjectSearcher("SELECT CommandLine FROM Win32_Process WHERE ProcessId = " + record.Pid))
            {
            search.Options.Timeout = TimeSpan.FromMilliseconds(500);
            using (var rows = search.Get())
            {
                foreach (ManagementObject row in rows)
                using (row)
                {
                    var command = row["CommandLine"] as string;
                    if (string.IsNullOrEmpty(command)) throw new StartupIdentityUnavailableException("Server command-line identity is not available yet.");
                    if (Regex.IsMatch(command, @"(?:^|\s)-dedicated(?=\s|$)", RegexOptions.IgnoreCase) && HasNonce(command, record.Instance) &&
                        Regex.Matches(command, @"(?:^|\s)net_port\s+(\d+)(?=\s|$)").Count == 1 &&
                        Regex.IsMatch(command, @"(?:^|\s)net_port\s+" + record.Port + @"(?=\s|$)")) return;
                    throw new InvalidOperationException("The running server does not match this Manager launch.");
                }
            }
            }
            throw new StartupIdentityUnavailableException("Server command-line identity is not available yet.");
        }
    }
}
