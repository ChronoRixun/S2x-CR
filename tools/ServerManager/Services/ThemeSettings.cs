using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;

namespace S2x.ServerManager.Services
{
    /// <summary>The theme pack, in the design's order, then the accessibility theme.</summary>
    public enum ThemeMode { FieldOps, Undead, Phosphor, Outrun, PackAPunch, NightVision, Arcade, Prestige, HighContrast }

    /// <summary>What settings.json holds: the theme and whether Textures &amp; glow is on.</summary>
    public sealed class ThemePreference
    {
        public ThemeMode Theme { get; set; } = ThemeCatalog.Default;
        public bool Effects { get; set; } = true;
    }

    /// <summary>
    /// Per-user appearance settings, %LOCALAPPDATA%\S2x\ServerManager\settings.json:
    /// {"Theme":"Undead","Effects":true}. 1.1.0 files hold only Theme, with Classic, Light or
    /// HighContrast; Classic reads as Phosphor and Light as Field Ops. The file is rewritten
    /// only when the user changes something, so reading never writes.
    /// </summary>
    public static class ThemeSettings
    {
        public static string DefaultPath => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "S2x", "ServerManager", "settings.json");

        /// <summary>The theme alone, as 1.1.0 read it.</summary>
        public static ThemeMode Load(string path, out string warning)
        {
            return LoadPreference(path, out warning).Theme;
        }

        /// <summary>
        /// Reads the preference, never throwing for a bad file: a missing file is the default
        /// (Undead, effects on); an unreadable or unrecognized one is the default with a warning.
        /// A readable theme with a bad Effects value keeps the theme.
        /// </summary>
        public static ThemePreference LoadPreference(string path, out string warning)
        {
            warning = null;
            var preference = new ThemePreference();
            var name = ThemeCatalog.Get(ThemeCatalog.Default).Name;
            try
            {
                if (!File.Exists(path)) return preference;
                var values = new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path)) as Dictionary<string, object>;
                if (values == null) { warning = "The saved theme was not recognized. " + name + " is selected."; return preference; }
                object effects;
                if (values.TryGetValue("Effects", out effects) && effects is bool) preference.Effects = (bool)effects;
                object theme;
                ThemeMode mode;
                bool migrated;
                if (values.TryGetValue("Theme", out theme) && ThemeCatalog.TryParse(theme as string, out mode, out migrated))
                {
                    preference.Theme = mode;
                    return preference;
                }
                warning = "The saved theme was not recognized. " + name + " is selected.";
            }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException || ex is ArgumentException || ex is InvalidOperationException)
            {
                warning = "The saved theme could not be read. " + name + " is selected. " + ex.Message;
            }
            return preference;
        }

        public static void Save(string path, ThemeMode mode)
        {
            Save(path, mode, true);
        }

        /// <summary>
        /// Written beside itself and swapped in, so an interrupted write never leaves half a file.
        /// Keys this version does not know are carried over. A failure throws and changes nothing.
        /// </summary>
        public static void Save(string path, ThemeMode mode, bool effects)
        {
            if (!Enum.IsDefined(typeof(ThemeMode), mode)) throw new ArgumentOutOfRangeException(nameof(mode));
            var fullPath = Path.GetFullPath(path);
            Directory.CreateDirectory(Path.GetDirectoryName(fullPath));
            var values = Existing(fullPath);
            values["Theme"] = mode.ToString();
            values["Effects"] = effects;
            var temporary = fullPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                File.WriteAllText(temporary, new JavaScriptSerializer().Serialize(values), new UTF8Encoding(false));
                if (File.Exists(fullPath)) File.Replace(temporary, fullPath, null);
                else File.Move(temporary, fullPath);
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }

        /// <summary>The keys already in the file, or none when it is missing or not a JSON object.</summary>
        private static Dictionary<string, object> Existing(string fullPath)
        {
            try
            {
                if (File.Exists(fullPath))
                {
                    var values = new JavaScriptSerializer().DeserializeObject(File.ReadAllText(fullPath)) as Dictionary<string, object>;
                    if (values != null) return new Dictionary<string, object>(values, StringComparer.Ordinal);
                }
            }
            catch (Exception ex) when (ex is ArgumentException || ex is InvalidOperationException) { }
            return new Dictionary<string, object>(StringComparer.Ordinal);
        }
    }
}
