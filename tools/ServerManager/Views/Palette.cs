using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Windows.Data;
using System.Windows.Media;
using S2x.ServerManager.Services;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// Live colour tokens for the parts a card decides in code. ThemeManager puts the same
    /// brushes in the application resources under the same keys, and a theme change sets their
    /// colours in place: a brush a view model handed out keeps being the right colour.
    /// </summary>
    internal static class Palette
    {
        private static readonly Dictionary<SolidColorBrush, ColorSignal> Signals = new Dictionary<SolidColorBrush, ColorSignal>();

        /// <summary>Every solid token, by resource key.</summary>
        internal static readonly Dictionary<string, SolidColorBrush> Tokens = new Dictionary<string, SolidColorBrush>();

        public static readonly Brush Accent = Token("Accent");
        public static readonly Brush AccentHot = Token("AccentHot");
        public static readonly Brush Ok = Token("Ok");
        public static readonly Brush Danger = Token("Danger");
        public static readonly Brush Ink = Token("Ink");
        public static readonly Brush Label = Token("Label");
        public static readonly Brush Muted = Token("Muted");
        public static readonly Brush Dim = Token("Dim");
        public static readonly Brush Faint = Token("Faint");
        public static readonly Brush Off = Token("StatusOff");
        public static readonly Brush Line = Token("Line");
        public static readonly Brush Edge = Token("Edge");
        public static readonly Brush EdgeHot = Token("EdgeHot");
        public static readonly Brush Bar = Token("Bar");
        public static readonly Brush Field = Token("Field");
        public static readonly Brush Row = Token("Row");
        public static readonly Brush Panel = Token("Panel");
        public static readonly Brush Surface = Token("Surface");
        public static readonly Brush FillInk = Token("FillInk");
        public static readonly Brush TagBg = Token("TagBg");
        public static readonly Brush TagEdge = Token("TagEdge");
        public static readonly Brush DangerBg = Token("DangerBg");
        public static readonly Brush DangerEdge = Token("DangerEdge");
        /// <summary>The status colours at the theme's badge tint (transparent where the badge has none).</summary>
        public static readonly Brush OkTint = Token("OkTint");
        public static readonly Brush AccentTint = Token("AccentTint");
        public static readonly Brush DangerTint = Token("DangerTint");
        public static readonly Brush MutedTint = Token("MutedTint");
        public static readonly Brush Transparent = Brushes.Transparent;

        static Palette()
        {
            // The rest of the keys have no field; they still get a live brush for the resources.
            // Every token starts as the default theme, so anything drawn before ThemeManager
            // starts (a test, a designer) still has its colours.
            var colors = ThemeResources.DeriveColors(ThemeCatalog.Get(ThemeCatalog.Default));
            foreach (var key in ThemeResources.ColorKeys) SetColor(Token(key), colors[key]);
        }

        internal static readonly Dictionary<string, SolidColorBrush> GameTextBrushes = new Dictionary<string, SolidColorBrush>();
        /// <summary>True when the window is light, so bright name colours are darkened instead.</summary>
        internal static bool LightTheme;

        public static Brush GameText(string hex)
        {
            SolidColorBrush brush;
            if (!GameTextBrushes.TryGetValue(hex, out brush))
            {
                brush = Mutable(TextColor(hex));
                GameTextBrushes.Add(hex, brush);
            }
            return brush;
        }

        internal static Color TextColor(string hex)
        {
            var color = (Color)ColorConverter.ConvertFromString(hex);
            // Preserve the colour identity while keeping names readable on both surfaces.
            var brightness = (color.R * 299 + color.G * 587 + color.B * 114) / 1000;
            if (LightTheme && brightness > 115)
            {
                var scale = 105.0 / brightness;
                return Color.FromRgb((byte)(color.R * scale), (byte)(color.G * scale), (byte)(color.B * scale));
            }
            if (!LightTheme && brightness < 75)
                return Color.FromRgb((byte)Math.Max((int)color.R, 155), (byte)Math.Max((int)color.G, 155), (byte)Math.Max((int)color.B, 155));
            return color;
        }

        private sealed class ColorSignal : INotifyPropertyChanged
        {
            private Color _value;
            public Color Value { get { return _value; } set { _value = value; PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Value))); } }
            public event PropertyChangedEventHandler PropertyChanged;
        }

        private static SolidColorBrush Token(string key)
        {
            SolidColorBrush brush;
            if (!Tokens.TryGetValue(key, out brush)) Tokens[key] = brush = Mutable(Colors.Transparent);
            return brush;
        }

        internal static SolidColorBrush Mutable(Color color)
        {
            var signal = new ColorSignal { Value = color };
            var brush = new SolidColorBrush();
            // A binding keeps WPF resource/style sealing from freezing this live token.
            BindingOperations.SetBinding(brush, SolidColorBrush.ColorProperty, new Binding(nameof(ColorSignal.Value)) { Source = signal });
            Signals.Add(brush, signal);
            return brush;
        }

        internal static void SetColor(SolidColorBrush brush, Color color) { Signals[brush].Value = color; }

        public static Brush Frozen(string hex)
        {
            var brush = new SolidColorBrush((Color)ColorConverter.ConvertFromString(hex));
            brush.Freeze();
            return brush;
        }
    }
}
