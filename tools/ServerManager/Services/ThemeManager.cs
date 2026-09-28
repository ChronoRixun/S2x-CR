using System;
using System.Collections.Generic;
using System.Reflection;
using System.Windows;
using System.Windows.Media;
using S2x.ServerManager.Views;

namespace S2x.ServerManager.Services
{
    public static class ThemeManager
    {
        private static readonly Dictionary<string, SolidColorBrush> Brushes = new Dictionary<string, SolidColorBrush>();
        private static readonly Dictionary<string, Color> Classic = new Dictionary<string, Color>();
        private static string _settingsPath;
        public static ThemeMode Current { get; private set; }
        public static string LoadWarning { get; private set; }
        public static event EventHandler Changed;

        // Supply a private test path to avoid touching the user's saved preference.
        public static void Initialize(string settingsPath = null)
        {
            var app = Application.Current ?? throw new InvalidOperationException("A WPF Application is required.");
            app.Dispatcher.VerifyAccess();
            if (Brushes.Count == 0)
            {
                var keys = "Accent AccentHot Ok Danger Ink Label Muted Dim Faint Ghost StatusOff Line Edge EdgeHot BtnEdge IconEdge Field Row Panel Bar Knob TagBg TagEdge DangerBg DangerEdge Divider WindowEdge".Split(' ');
                foreach (var key in keys)
                {
                    var original = (SolidColorBrush)app.FindResource(key);
                    Classic[key] = original.Color;
                    var field = typeof(Palette).GetField(key == "StatusOff" ? "Off" : key, BindingFlags.Public | BindingFlags.Static);
                    var brush = field == null ? Palette.Mutable(original.Color) : (SolidColorBrush)field.GetValue(null);
                    Brushes[key] = brush;
                    app.Resources[key] = brush;
                }
            }
            _settingsPath = settingsPath ?? ThemeSettings.DefaultPath;
            string warning;
            var mode = ThemeSettings.Load(_settingsPath, out warning);
            LoadWarning = warning;
            Apply(mode, false);
        }

        public static void Apply(ThemeMode mode, bool persist = true)
        {
            if (Brushes.Count == 0) throw new InvalidOperationException("Initialize themes before applying them.");
            Application.Current.Dispatcher.VerifyAccess();
            if (!Enum.IsDefined(typeof(ThemeMode), mode)) throw new ArgumentOutOfRangeException(nameof(mode));
            // Save first: an unwritable preferences file leaves the visible selection intact.
            if (persist) ThemeSettings.Save(_settingsPath, mode);
            var values = mode == ThemeMode.Light ? Light : mode == ThemeMode.HighContrast ? Contrast : null;
            foreach (var entry in Brushes)
                Palette.SetColor(entry.Value, values == null ? Classic[entry.Key] : (Color)ColorConverter.ConvertFromString(values[entry.Key]));
            Palette.LightTheme = mode == ThemeMode.Light;
            foreach (var entry in Palette.GameTextBrushes) Palette.SetColor(entry.Value, Palette.TextColor(entry.Key));
            Current = mode;
            Changed?.Invoke(null, EventArgs.Empty);
        }

        private static Dictionary<string, string> Colors(string[] values)
        {
            var keys = "Accent AccentHot Ok Danger Ink Label Muted Dim Faint Ghost StatusOff Line Edge EdgeHot BtnEdge IconEdge Field Row Panel Bar Knob TagBg TagEdge DangerBg DangerEdge Divider WindowEdge".Split(' ');
            var colors = new Dictionary<string, string>();
            for (var i = 0; i < keys.Length; i++) colors.Add(keys[i], values[i]);
            return colors;
        }
        private static readonly Dictionary<string, string> Light = Colors(new[] {
            "#875000", "#A36100", "#176738", "#AC211B", "#182027", "#37434E", "#45535F", "#52606A", "#596570", "#69757E", "#68737B",
            "#CFD5DB", "#AAB5BF", "#657786", "#8E9CA8", "#AAB5BF", "#FFFFFF", "#E8EDF2", "#F7F9FB", "#E0E6EC", "#FFFFFF", "#FFF0D4", "#AD833C", "#FFE5E1", "#B66C66", "#C4CDD5", "#8997A3"
        });
        private static readonly Dictionary<string, string> Contrast = Colors(new[] {
            "#FFD800", "#FFF06A", "#76FF9D", "#FF9393", "#FFFFFF", "#F3F3F3", "#E0E0E0", "#CCCCCC", "#BBBBBB", "#AAAAAA", "#AAAAAA",
            "#777777", "#AAAAAA", "#FFFFFF", "#AAAAAA", "#AAAAAA", "#111111", "#222222", "#000000", "#080808", "#000000", "#292400", "#FFD800", "#300000", "#FF9393", "#AAAAAA", "#FFFFFF"
        });
    }
}
