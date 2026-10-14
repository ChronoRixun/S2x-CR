using System;
using System.Linq;
using System.Windows;
using System.Windows.Media;
using S2x.ServerManager.Views;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// The visible theme. Solid colours are the live Palette brushes, recoloured in place so
    /// anything holding one follows; everything else (the fill, effects, radii, badge, fonts,
    /// sizes) is replaced under its resource key, which every view reads with DynamicResource.
    /// </summary>
    public static class ThemeManager
    {
        private static string _settingsPath;
        private static bool _initialized;

        public static ThemeMode Current { get; private set; } = ThemeCatalog.Default;
        /// <summary>The user's Textures &amp; glow setting (High contrast draws flat whatever it says).</summary>
        public static bool EffectsEnabled { get; private set; } = true;
        public static ThemeResources Resources { get; private set; }
        public static string LoadWarning { get; private set; }
        public static event EventHandler Changed;

        // Supply a private test path to avoid touching the user's saved preference.
        public static void Initialize(string settingsPath = null)
        {
            var app = Application.Current ?? throw new InvalidOperationException("A WPF Application is required.");
            app.Dispatcher.VerifyAccess();
            if (!_initialized)
            {
                foreach (var key in ThemeResources.ColorKeys) app.Resources[key] = Palette.Tokens[key];
                // Stock templates (the corner between two scrollbars) paint with the system's
                // light control colour; point it at the chrome token instead.
                app.Resources[SystemColors.ControlBrushKey] = Palette.Tokens["Bar"];
                _initialized = true;
            }
            _settingsPath = settingsPath ?? ThemeSettings.DefaultPath;
            string warning;
            var preference = ThemeSettings.LoadPreference(_settingsPath, out warning);
            LoadWarning = warning;
            Show(preference.Theme, preference.Effects);
        }

        /// <summary>Changes the theme, keeping the effects setting.</summary>
        public static void Apply(ThemeMode mode, bool persist = true)
        {
            Apply(mode, EffectsEnabled, persist);
        }

        /// <summary>Turns Textures &amp; glow on or off, keeping the theme.</summary>
        public static void SetEffects(bool enabled, bool persist = true)
        {
            Apply(Current, enabled, persist);
        }

        /// <summary>
        /// Saves first: an unwritable preferences file throws and leaves the visible theme and
        /// effects exactly as they were.
        /// </summary>
        public static void Apply(ThemeMode mode, bool effects, bool persist)
        {
            if (!_initialized) throw new InvalidOperationException("Initialize themes before applying them.");
            Application.Current.Dispatcher.VerifyAccess();
            if (!Enum.IsDefined(typeof(ThemeMode), mode)) throw new ArgumentOutOfRangeException(nameof(mode));
            if (persist) ThemeSettings.Save(_settingsPath, mode, effects);
            Show(mode, effects);
        }

        private static void Show(ThemeMode mode, bool effects)
        {
            var theme = ThemeResources.For(mode, effects);
            var resources = Application.Current.Resources;

            foreach (var key in ThemeResources.ColorKeys) Palette.SetColor(Palette.Tokens[key], theme.Colors[key]);
            Palette.LightTheme = ThemeResources.Luminance(theme.Colors["Panel"]) > 0.4;
            foreach (var entry in Palette.GameTextBrushes) Palette.SetColor(entry.Value, Palette.TextColor(entry.Key));

            resources["Fill"] = theme.Fill;
            resources["FillHot"] = theme.FillHot;
            // null when Textures & glow is off: the property falls back to no effect at once.
            resources["Glow"] = theme.Glow;
            resources["OkGlow"] = theme.OkGlow;
            resources["TextGlow"] = theme.TextGlow;
            resources["CardShadow"] = theme.CardShadow;

            resources["Radius"] = new CornerRadius(theme.Radius);
            resources["RadiusValue"] = theme.Radius;
            resources["RadiusBadge"] = new CornerRadius(theme.BadgeRadius);
            resources["RadiusBadgeValue"] = theme.BadgeRadius;

            var dashed = theme.BadgeStyle == "dashed";
            var twin = theme.BadgeStyle == "double";
            resources["BadgeBorder"] = new Thickness(twin ? 1 : theme.BadgeBorder);
            resources["BadgeInner"] = new Thickness(Math.Max(2, theme.BadgeBorder - 1));
            resources["BadgeSolidVisibility"] = dashed ? Visibility.Collapsed : Visibility.Visible;
            resources["BadgeDoubleVisibility"] = twin ? Visibility.Visible : Visibility.Collapsed;
            resources["BadgeDashedVisibility"] = dashed ? Visibility.Visible : Visibility.Collapsed;
            resources["BadgeTransform"] = theme.BadgeTransform;

            resources["FontDisplay"] = theme.Display.Family;
            resources["FontDisplayWeight"] = theme.Display.Weight;
            resources["FontDisplayStretch"] = theme.Display.Stretch;
            resources["FontBody"] = theme.Body.Family;
            resources["FontBodyStretch"] = theme.Body.Stretch;
            resources["FontLabel"] = theme.Label.Family;
            resources["FontLabelWeight"] = theme.Label.Weight;
            resources["FontLabelStretch"] = theme.Label.Stretch;
            resources["FontMono"] = theme.Mono.Family;
            foreach (var size in theme.Sizes) resources[size.Key] = size.Value;

            Current = mode;
            EffectsEnabled = effects;
            Resources = theme;
            Changed?.Invoke(null, EventArgs.Empty);
        }

        /// <summary>A token's current colour, for code that cannot bind (the tray menu).</summary>
        public static Color Token(string key)
        {
            return Resources != null && Resources.Colors.ContainsKey(key) ? Resources.Colors[key] : Colors.Gray;
        }
    }
}
