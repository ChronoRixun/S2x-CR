using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using Microsoft.Win32;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// Finds the Call of Duty: WWII folder that holds s2x.exe: the Steam registry entry, this
    /// exe's own folder walking up, the choice a launcher remembered, then a folder picker.
    /// The picked folder is remembered in the same file the PowerShell launcher uses.
    /// </summary>
    internal static class GameFolder
    {
        private const string SteamUninstallKey =
            @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 476600";

        public static string RememberedFile
        {
            get
            {
                return Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    @"s2x\launcher-gamedir.txt");
            }
        }

        public static bool Holds(string dir)
        {
            try { return !string.IsNullOrEmpty(dir) && File.Exists(Path.Combine(dir, "s2x.exe")); }
            catch { return false; }
        }

        /// <summary>The folder, or null when nothing was found without asking.</summary>
        public static string Find()
        {
            foreach (var candidate in Candidates())
                if (Holds(candidate)) return Path.GetFullPath(candidate);
            return null;
        }

        private static IEnumerable<string> Candidates()
        {
            yield return SteamInstallLocation();

            // The release ships this exe as <game>\s2x\tools\ServerManager, so walk up.
            var dir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
            for (int i = 0; i < 5 && !string.IsNullOrEmpty(dir); i++)
            {
                yield return dir;
                dir = Path.GetDirectoryName(dir);
            }

            yield return Remembered();
            yield return Environment.CurrentDirectory;
        }

        private static string SteamInstallLocation()
        {
            try
            {
                using (var key = Registry.LocalMachine.OpenSubKey(SteamUninstallKey))
                    if (key != null) return key.GetValue("InstallLocation") as string;
            }
            catch { }
            return null;
        }

        private static string Remembered()
        {
            try
            {
                var file = RememberedFile;
                if (File.Exists(file)) return File.ReadAllText(file).Trim();
            }
            catch { }
            return null;
        }

        public static void Remember(string dir)
        {
            try
            {
                var file = RememberedFile;
                Directory.CreateDirectory(Path.GetDirectoryName(file));
                File.WriteAllText(file, dir, new System.Text.UTF8Encoding(false));
            }
            catch { }
        }

        /// <summary>Last resort: ask. Returns null when the host cancels.</summary>
        public static string Ask()
        {
            using (var picker = new System.Windows.Forms.FolderBrowserDialog())
            {
                picker.Description = "Select the Call of Duty WWII folder that contains s2x.exe";
                picker.ShowNewFolderButton = false;
                if (picker.ShowDialog() == System.Windows.Forms.DialogResult.OK && Holds(picker.SelectedPath))
                {
                    Remember(picker.SelectedPath);
                    return picker.SelectedPath;
                }
            }
            return null;
        }

        public static string PresetDir(string gameDir) { return Path.Combine(gameDir, @"s2x\presets"); }
        public static string CfgPath(string gameDir, int port) { return Path.Combine(gameDir, @"s2x\server-" + port + ".cfg"); }
        public static string PidPath(string gameDir, int port) { return Path.Combine(gameDir, @"s2x\server-" + port + ".pid"); }
        public static string ExePath(string gameDir) { return Path.Combine(gameDir, "s2x.exe"); }
    }
}
