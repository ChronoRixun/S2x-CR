using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Windows.Data;
using System.Windows.Media;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// Live colour tokens for the parts a card decides in code. Theme.xaml carries
    /// the Classic defaults; ThemeManager shares these brushes with XAML resources.
    /// </summary>
    internal static class Palette
    {
        private static readonly Dictionary<SolidColorBrush, ColorSignal> Signals = new Dictionary<SolidColorBrush, ColorSignal>();
        public static readonly Brush Accent = Mutable("#FFE8A33D");
        public static readonly Brush AccentHot = Mutable("#FFF4BB66");
        public static readonly Brush Ok = Mutable("#FF5FBF7A");
        public static readonly Brush Danger = Mutable("#FFC7524A");
        public static readonly Brush Ink = Mutable("#FFE6E8EA");
        public static readonly Brush Label = Mutable("#FFA8B0B6");
        public static readonly Brush Muted = Mutable("#FF8A9299");
        public static readonly Brush Dim = Mutable("#FF6F777D");
        public static readonly Brush Faint = Mutable("#FF5C646B");
        public static readonly Brush Off = Mutable("#FF4A5157");
        public static readonly Brush Line = Mutable("#FF1C2024");
        public static readonly Brush Edge = Mutable("#FF262B30");
        public static readonly Brush EdgeHot = Mutable("#FF3A4147");
        public static readonly Brush Bar = Mutable("#FF0A0B0D");
        public static readonly Brush Field = Mutable("#FF16191C");
        public static readonly Brush Row = Mutable("#FF121417");
        public static readonly Brush Panel = Mutable("#FF0E1012");
        public static readonly Brush TagBg = Mutable("#FF1A1710");
        public static readonly Brush TagEdge = Mutable("#FF3A2F1C");
        public static readonly Brush DangerBg = Mutable("#FF1C1211");
        public static readonly Brush DangerEdge = Mutable("#FF4A2A28");
        public static readonly Brush Transparent = Brushes.Transparent;

        internal static readonly Dictionary<string, SolidColorBrush> GameTextBrushes = new Dictionary<string, SolidColorBrush>();
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
        private static Brush Mutable(string hex) { return Mutable((Color)ColorConverter.ConvertFromString(hex)); }
        public static Brush Frozen(string hex)
        {
            var brush = new SolidColorBrush((Color)ColorConverter.ConvertFromString(hex));
            brush.Freeze();
            return brush;
        }
    }
}
