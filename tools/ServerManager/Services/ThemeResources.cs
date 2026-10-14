using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Effects;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// A theme turned into what WPF draws with: the app's colour tokens, the fill, the effects
    /// (null when Textures &amp; glow is off), corner radii, the badge, and a font and size for
    /// each role. Built once per theme and effects setting; everything in it is frozen.
    /// </summary>
    public sealed class ThemeResources
    {
        /// <summary>The solid tokens the views and view models share by identity (see Palette).</summary>
        public static readonly string[] ColorKeys =
            ("Accent AccentHot Ok Danger Ink Label Muted Dim Faint Ghost StatusOff Line Edge EdgeHot BtnEdge IconEdge " +
             "Field Row Panel Bar Knob TagBg TagEdge DangerBg DangerEdge Divider WindowEdge " +
             "Surface FillInk OkTint AccentTint DangerTint MutedTint").Split(' ');

        /// <summary>Font size tokens: key, base px (the 1.1.0 sizes), role.</summary>
        public static readonly IList<Tuple<string, double, string>> SizeKeys = new List<Tuple<string, double, string>>
        {
            Tuple.Create("SizeBody", 13.0, "body"),
            Tuple.Create("SizeBodyL", 14.0, "body"),
            Tuple.Create("SizeBodyS", 12.0, "body"),
            Tuple.Create("SizeBodyXS", 11.0, "body"),
            Tuple.Create("SizeHeading", 16.0, "body"),
            Tuple.Create("SizeLabel", 10.0, "label"),
            Tuple.Create("SizeLabelL", 10.5, "label"),
            Tuple.Create("SizeButton", 11.0, "label"),
            Tuple.Create("SizeMono", 11.0, "mono"),
            Tuple.Create("SizeMonoS", 10.0, "mono"),
            Tuple.Create("SizeMonoL", 12.5, "mono"),
            Tuple.Create("SizePort", 15.0, "mono"),
            Tuple.Create("SizeMonoXL", 20.0, "mono"),
            Tuple.Create("SizeStat", 26.0, "mono"),
            Tuple.Create("SizeBrand", 15.0, "display"),
            Tuple.Create("SizeTile", 13.0, "display"),
            Tuple.Create("SizeDisplay", 21.0, "display"),
            Tuple.Create("SizeDisplayL", 28.0, "display"),
        };

        private static readonly Dictionary<string, ThemeResources> Cache = new Dictionary<string, ThemeResources>();

        public ThemeDefinition Theme { get; private set; }
        public bool Effects { get; private set; }
        public IDictionary<string, Color> Colors { get; private set; }
        public Brush Fill { get; private set; }
        public Brush FillHot { get; private set; }
        public Effect Glow { get; private set; }
        public Effect OkGlow { get; private set; }
        public Effect TextGlow { get; private set; }
        public Effect CardShadow { get; private set; }
        public double Radius { get; private set; }
        public double BadgeRadius { get; private set; }
        public double BadgeBorder { get; private set; }
        public string BadgeStyle { get; private set; }
        public Transform BadgeTransform { get; private set; }
        public double BadgeAlpha { get; private set; }
        public ResolvedFont Display { get; private set; }
        public ResolvedFont Body { get; private set; }
        public ResolvedFont Label { get; private set; }
        public ResolvedFont Mono { get; private set; }
        public double DisplayScale { get; private set; }
        public double BodyScale { get; private set; }
        public double LabelScale { get; private set; }
        public double MonoScale { get; private set; }
        public IDictionary<string, double> Sizes { get; private set; }

        public static ThemeResources For(ThemeMode mode, bool effects)
        {
            var theme = ThemeCatalog.Get(mode);
            var fx = effects && theme.HasEffects;
            var key = mode + (fx ? "|fx" : "|flat");
            ThemeResources built;
            if (!Cache.TryGetValue(key, out built)) Cache[key] = built = new ThemeResources(theme, fx);
            return built;
        }

        private ThemeResources(ThemeDefinition theme, bool effects)
        {
            Theme = theme;
            Effects = effects;
            Colors = DeriveColors(theme);
            Fill = Css.Fill(theme.Fill);
            FillHot = Css.Brighter(Fill, 1.15);
            if (effects)
            {
                Glow = Css.Shadow(theme.Glow);
                OkGlow = Css.Shadow(theme.OkGlow);
                TextGlow = Css.Shadow(theme.TextShadow);
                CardShadow = Css.Shadow(theme.Shadow);
            }
            Radius = Css.Px(theme.Radius);
            BadgeRadius = Css.Px(theme.BadgeRadius);
            BadgeBorder = Math.Max(1, Css.Px(theme.BadgeBorderWidth));
            BadgeStyle = (theme.BadgeBorderStyle ?? "solid").Trim().ToLowerInvariant();
            BadgeTransform = Css.ParseTransform(theme.BadgeTransform);
            BadgeAlpha = Css.MixAlpha(theme.BadgeBackground);

            Display = ThemeFonts.Resolve(theme.DisplayFont, true);
            Body = ThemeFonts.Resolve(theme.BodyFont, false);
            Label = ThemeFonts.Resolve(theme.LabelFont, true);
            Mono = ThemeFonts.Resolve(theme.MonoFont, false);
            DisplayScale = theme.DisplayScale * Display.Scale;
            BodyScale = theme.BodyScale * Body.Scale;
            LabelScale = theme.LabelScale * Label.Scale;
            // The pack sizes mono text by the body scale (calc(12px * var(--k))).
            MonoScale = theme.BodyScale * Mono.Scale;

            var sizes = new Dictionary<string, double>();
            foreach (var size in SizeKeys)
            {
                var scale = size.Item3 == "body" ? BodyScale : size.Item3 == "label" ? LabelScale : size.Item3 == "mono" ? MonoScale : DisplayScale;
                sizes[size.Item1] = Math.Round(size.Item2 * scale * 2, MidpointRounding.AwayFromZero) / 2;
            }
            Sizes = sizes;
        }

        /// <summary>
        /// The 1.1.0 token set from the pack's ten colours. The old keys keep their jobs: Panel is
        /// the window (the pack's bg), Bar the chrome and cards (the pack's panel), Ink body text
        /// (the pack's text) and Knob the text on a filled button (the pack's ink). The in-between
        /// greys are mixes, so every theme gets a full ramp from its own colours.
        /// </summary>
        public static Dictionary<string, Color> DeriveColors(ThemeDefinition t)
        {
            var bg = Css.ParseColor(t.Bg);
            var panel = Css.ParseColor(t.Panel);
            var surface = Css.ParseColor(t.Surface);
            var line = Css.ParseColor(t.Line);
            var text = Css.ParseColor(t.Text);
            var muted = Css.ParseColor(t.Muted);
            var accent = Css.ParseColor(t.Accent);
            var ink = Css.ParseColor(t.Ink);
            var ok = Css.ParseColor(t.Ok);
            var bad = Css.ParseColor(t.Bad);
            var alpha = Css.MixAlpha(t.BadgeBackground);

            var c = new Dictionary<string, Color>
            {
                { "Accent", accent },
                { "AccentHot", Mix(accent, System.Windows.Media.Colors.White, 0.2) },
                { "Ok", ok },
                { "Danger", bad },
                { "Ink", text },
                { "Label", Mix(text, muted, 0.45) },
                { "Muted", muted },
                { "Dim", Mix(muted, panel, 0.07) },
                { "Faint", Mix(muted, panel, 0.14) },
                { "Ghost", Mix(line, muted, 0.35) },
                { "StatusOff", Mix(muted, panel, 0.45) },
                { "Line", line },
                { "Edge", Mix(line, muted, 0.12) },
                { "EdgeHot", Mix(line, muted, 0.45) },
                { "BtnEdge", line },
                { "IconEdge", Mix(line, panel, 0.3) },
                { "Field", surface },
                { "Row", Mix(panel, surface, 0.65) },
                { "Panel", bg },
                { "Bar", panel },
                { "Knob", ink },
                { "TagBg", Mix(panel, accent, 0.12) },
                { "TagEdge", Mix(panel, accent, 0.45) },
                { "DangerBg", Mix(panel, bad, 0.10) },
                { "DangerEdge", Mix(panel, bad, 0.5) },
                { "Divider", line },
                { "WindowEdge", Mix(line, muted, 0.25) },
                { "Surface", surface },
                { "FillInk", ink },
                { "OkTint", WithAlpha(ok, alpha) },
                { "AccentTint", WithAlpha(accent, alpha) },
                { "DangerTint", WithAlpha(bad, alpha) },
                { "MutedTint", WithAlpha(muted, alpha) },
            };
            foreach (var entry in t.Overrides) c[entry.Key] = Css.ParseColor(entry.Value);
            return c;
        }

        public static Color Mix(Color from, Color to, double amount)
        {
            Func<byte, byte, byte> channel = (a, b) => (byte)Math.Round(a + (b - a) * amount);
            return Color.FromArgb(channel(from.A, to.A), channel(from.R, to.R), channel(from.G, to.G), channel(from.B, to.B));
        }

        private static Color WithAlpha(Color color, double alpha)
        {
            return Color.FromArgb((byte)Math.Round(255 * Math.Max(0, Math.Min(1, alpha))), color.R, color.G, color.B);
        }

        /// <summary>WCAG relative luminance, for the contrast checks and the game-colour guard.</summary>
        public static double Luminance(Color color)
        {
            Func<byte, double> linear = v =>
            {
                var s = v / 255.0;
                return s <= 0.03928 ? s / 12.92 : Math.Pow((s + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * linear(color.R) + 0.7152 * linear(color.G) + 0.0722 * linear(color.B);
        }

        public static double Contrast(Color a, Color b)
        {
            var la = Luminance(a);
            var lb = Luminance(b);
            return (Math.Max(la, lb) + 0.05) / (Math.Min(la, lb) + 0.05);
        }
    }
}
