using System;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;

namespace S2x.ServerManager.Services
{
    public enum ThemeMode { Classic, Light, HighContrast }

    public sealed class ThemeSettings
    {
        public string Theme { get; set; } = "Classic";
        public static string DefaultPath => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "S2x", "ServerManager", "settings.json");

        public static ThemeMode Load(string path, out string warning)
        {
            warning = null;
            try
            {
                if (!File.Exists(path)) return ThemeMode.Classic;
                var settings = new JavaScriptSerializer().Deserialize<ThemeSettings>(File.ReadAllText(path));
                ThemeMode mode;
                if (settings != null && Enum.TryParse(settings.Theme, out mode) && Enum.IsDefined(typeof(ThemeMode), mode)) return mode;
                warning = "The saved theme was not recognized. Classic is selected.";
            }
            catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException || ex is ArgumentException || ex is InvalidOperationException)
            { warning = "The saved theme could not be read. Classic is selected. " + ex.Message; }
            return ThemeMode.Classic;
        }

        public static void Save(string path, ThemeMode mode)
        {
            if (!Enum.IsDefined(typeof(ThemeMode), mode)) throw new ArgumentOutOfRangeException(nameof(mode));
            var fullPath = Path.GetFullPath(path);
            Directory.CreateDirectory(Path.GetDirectoryName(fullPath));
            var temporary = fullPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                File.WriteAllText(temporary, new JavaScriptSerializer().Serialize(new ThemeSettings { Theme = mode.ToString() }), new UTF8Encoding(false));
                if (File.Exists(fullPath)) File.Replace(temporary, fullPath, null);
                else File.Move(temporary, fullPath);
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }
    }
}
