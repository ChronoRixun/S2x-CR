using System.Windows.Media;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// The launcher's colour tokens, for the parts a card decides in code. Theme.xaml carries
    /// the same values for XAML; these are the ones the view models hand out.
    /// </summary>
    internal static class Palette
    {
        public static readonly Brush Accent = Frozen("#FFE8A33D");
        public static readonly Brush AccentHot = Frozen("#FFF4BB66");
        public static readonly Brush Ok = Frozen("#FF5FBF7A");
        public static readonly Brush Danger = Frozen("#FFC7524A");
        public static readonly Brush Ink = Frozen("#FFE6E8EA");
        public static readonly Brush Label = Frozen("#FFA8B0B6");
        public static readonly Brush Muted = Frozen("#FF8A9299");
        public static readonly Brush Dim = Frozen("#FF6F777D");
        public static readonly Brush Faint = Frozen("#FF5C646B");
        public static readonly Brush Off = Frozen("#FF4A5157");
        public static readonly Brush Line = Frozen("#FF1C2024");
        public static readonly Brush Edge = Frozen("#FF262B30");
        public static readonly Brush EdgeHot = Frozen("#FF3A4147");
        public static readonly Brush Bar = Frozen("#FF0A0B0D");
        public static readonly Brush Panel = Frozen("#FF0E1012");
        public static readonly Brush TagBg = Frozen("#FF1A1710");
        public static readonly Brush TagEdge = Frozen("#FF3A2F1C");
        public static readonly Brush DangerBg = Frozen("#FF1C1211");
        public static readonly Brush DangerEdge = Frozen("#FF4A2A28");
        public static readonly Brush Transparent = Brushes.Transparent;

        public static Brush Frozen(string hex)
        {
            var brush = new SolidColorBrush((Color)ColorConverter.ConvertFromString(hex));
            brush.Freeze();
            return brush;
        }
    }
}
