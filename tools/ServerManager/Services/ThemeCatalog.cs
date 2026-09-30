using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Media;
using S2x.ServerManager.Views;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// One theme as the design (design/S2x Theme Pack.dc.html, THEMES) writes it: the same
    /// ten colours, fill, fonts, scales, radius, effects and badge, kept as the CSS strings they
    /// are there. ThemeResources turns them into WPF objects. Textures and overlays are CSS
    /// background stacks, which have no WPF parser here; they are spelled out as ThemeLayer
    /// lists beside the CSS they came from.
    /// </summary>
    public sealed class ThemeDefinition
    {
        public ThemeMode Mode { get; set; }
        public string Id { get; set; }
        public string Name { get; set; }
        public string Tagline { get; set; }

        public string Bg { get; set; }
        public string Panel { get; set; }
        public string Surface { get; set; }
        public string Line { get; set; }
        public string Text { get; set; }
        public string Muted { get; set; }
        public string Accent { get; set; }
        public string Ink { get; set; }
        public string Ok { get; set; }
        public string Bad { get; set; }
        public string Fill { get; set; }

        public string DisplayFont { get; set; }
        public string BodyFont { get; set; }
        public string LabelFont { get; set; }
        public string MonoFont { get; set; }
        public double BodyScale { get; set; } = 1;
        public double DisplayScale { get; set; } = 1;
        public double LabelScale { get; set; } = 1;

        public string Radius { get; set; }
        /// <summary>Kept for reference only: WPF has no letter spacing (see README).</summary>
        public string LetterSpacing { get; set; }

        public string Glow { get; set; } = "none";
        public string OkGlow { get; set; } = "none";
        public string TextShadow { get; set; } = "none";
        public string Shadow { get; set; } = "none";
        /// <summary>The design's CSS, for reference; TextureLayers is what is drawn.</summary>
        public string Texture { get; set; } = "none";
        public string Overlay { get; set; } = "none";
        public IList<ThemeLayer> TextureLayers { get; set; } = new List<ThemeLayer>();
        public IList<ThemeLayer> OverlayLayers { get; set; } = new List<ThemeLayer>();

        public string BadgeBorderWidth { get; set; } = "1px";
        public string BadgeBorderStyle { get; set; } = "solid";
        public string BadgeRadius { get; set; } = "0px";
        public string BadgeTransform { get; set; } = "none";
        public string BadgeBackground { get; set; } = "transparent";

        /// <summary>Token colours set by hand instead of derived (High contrast keeps its tuned table).</summary>
        public IDictionary<string, string> Overrides { get; set; } = new Dictionary<string, string>();

        /// <summary>High contrast is always flat: the Textures &amp; glow switch does not apply.</summary>
        public bool HasEffects { get { return Mode != ThemeMode.HighContrast; } }
    }

    public static class ThemeCatalog
    {
        public const ThemeMode Default = ThemeMode.Undead;

        /// <summary>
        /// Overlay vignettes are drawn at half the design's alpha. The design darkens the corners
        /// of a mock-up; this window keeps its brand, first stat and close button there, and at
        /// full strength (Night Vision's is 80% black) they all but disappear.
        /// </summary>
        public const double VignetteStrength = 0.5;

        public static readonly IList<ThemeDefinition> All = Build();

        public static ThemeDefinition Get(ThemeMode mode)
        {
            return All.First(t => t.Mode == mode);
        }

        /// <summary>
        /// Reads a saved or typed theme name. Accepts the enum names (FieldOps), the design's ids
        /// (field, packapunch), display names ("Field Ops"), and the 1.1.0 names: Classic became
        /// Phosphor (dark amber) and Light became Field Ops. <paramref name="migrated"/> says a
        /// retired name was mapped.
        /// </summary>
        public static bool TryParse(string text, out ThemeMode mode, out bool migrated)
        {
            mode = Default;
            migrated = false;
            var value = (text ?? "").Trim();
            if (value.Length == 0) return false;
            if (string.Equals(value, "Classic", StringComparison.OrdinalIgnoreCase)) { mode = ThemeMode.Phosphor; migrated = true; return true; }
            if (string.Equals(value, "Light", StringComparison.OrdinalIgnoreCase)) { mode = ThemeMode.FieldOps; migrated = true; return true; }
            var squashed = Squash(value);
            foreach (var theme in All)
            {
                if (Squash(theme.Mode.ToString()) == squashed || Squash(theme.Id) == squashed || Squash(theme.Name) == squashed)
                {
                    mode = theme.Mode;
                    return true;
                }
            }
            return false;
        }

        public static string Names
        {
            get { return string.Join(", ", All.Select(t => t.Mode.ToString())); }
        }

        private static string Squash(string text)
        {
            return new string((text ?? "").Where(char.IsLetterOrDigit).ToArray()).ToLowerInvariant();
        }

        private static IList<ThemeDefinition> Build()
        {
            var list = new List<ThemeDefinition>();

            list.Add(new ThemeDefinition
            {
                Mode = ThemeMode.FieldOps, Id = "field", Name = "Field Ops", Tagline = "Stenciled olive drab. Issued, not bought.",
                Bg = "#1d1f14", Panel = "#262918", Surface = "#303420", Line = "#4b4f34", Text = "#e6dfc1", Muted = "#a39d7c",
                Accent = "#d8a93a", Ink = "#17170c", Ok = "#a9c75a", Bad = "#d0492f", Fill = "#d8a93a",
                DisplayFont = "Black Ops One", BodyFont = "Barlow Condensed", LabelFont = "Black Ops One", MonoFont = "Special Elite",
                BodyScale = 1.1, DisplayScale = 1, LabelScale = 1, Radius = "2px", LetterSpacing = ".16em",
                Glow = "0 2px 0 rgba(0,0,0,.45)", OkGlow = "none", TextShadow = "none",
                Shadow = "inset 0 0 0 1px rgba(0,0,0,.35), 0 3px 0 rgba(0,0,0,.35)",
                Texture = "repeating-linear-gradient(45deg, rgba(230,223,193,.025) 0 2px, transparent 2px 7px), repeating-linear-gradient(-45deg, rgba(0,0,0,.08) 0 2px, transparent 2px 7px)",
                TextureLayers = Layers(
                    new StripeLayer(45, "rgba(230,223,193,.025)", 2, 7),
                    new StripeLayer(-45, "rgba(0,0,0,.08)", 2, 7)),
                Overlay = "radial-gradient(ellipse at center, transparent 55%, rgba(0,0,0,.45))",
                OverlayLayers = Layers(RadialLayer.Ellipse(0.5, 0.5, "transparent 55%", "rgba(0,0,0,.45)").Strength(VignetteStrength)),
                BadgeBorderWidth = "2px", BadgeBorderStyle = "solid", BadgeRadius = "1px", BadgeTransform = "rotate(-2deg)", BadgeBackground = "transparent",
            });

            list.Add(new ThemeDefinition
            {
                Mode = ThemeMode.Undead, Id = "undead", Name = "Undead", Tagline = "Rust, bone, and something still moving.",
                Bg = "#0f0b0a", Panel = "#181110", Surface = "#211715", Line = "#3d2621", Text = "#eadcd5", Muted = "#a08378",
                Accent = "#e0313b", Ink = "#fff2ee", Ok = "#86ff5c", Bad = "#ff4436", Fill = "linear-gradient(180deg,#d91f2b,#8e0c14)",
                DisplayFont = "Creepster", BodyFont = "Oswald", LabelFont = "Oswald", MonoFont = "JetBrains Mono",
                BodyScale = 1, DisplayScale = 1.15, LabelScale = 1.05, Radius = "3px", LetterSpacing = ".14em",
                Glow = "0 0 22px rgba(195,20,31,.55)", OkGlow = "0 0 14px rgba(134,255,92,.5)", TextShadow = "none",
                Shadow = "0 10px 30px rgba(0,0,0,.5)",
                Texture = "radial-gradient(ellipse at 15% -10%, rgba(195,20,31,.22), transparent 45%), radial-gradient(ellipse at 100% 110%, rgba(134,255,92,.10), transparent 45%), repeating-radial-gradient(circle at 70% 30%, rgba(120,60,30,.05) 0 3px, transparent 3px 11px)",
                TextureLayers = Layers(
                    new RingLayer(0.70, 0.30, "rgba(120,60,30,.05)", 3, 11),
                    RadialLayer.Ellipse(1.00, 1.10, "rgba(134,255,92,.10)", "transparent 45%"),
                    RadialLayer.Ellipse(0.15, -0.10, "rgba(195,20,31,.22)", "transparent 45%")),
                Overlay = "radial-gradient(ellipse at center, transparent 50%, rgba(0,0,0,.6))",
                OverlayLayers = Layers(RadialLayer.Ellipse(0.5, 0.5, "transparent 50%", "rgba(0,0,0,.6)").Strength(VignetteStrength)),
                BadgeBorderWidth = "1px", BadgeBorderStyle = "solid", BadgeRadius = "2px", BadgeTransform = "none",
                BadgeBackground = "color-mix(in oklab, currentColor 14%, transparent)",
            });

            list.Add(new ThemeDefinition
            {
                Mode = ThemeMode.Phosphor, Id = "phosphor", Name = "Phosphor", Tagline = "Amber tube, 80 columns, warm to the touch.",
                Bg = "#0d0800", Panel = "#130b01", Surface = "#1b1002", Line = "#5a3706", Text = "#ffb640", Muted = "#c0852c",
                Accent = "#ffb000", Ink = "#0d0800", Ok = "#ffe08a", Bad = "#ff6a3d", Fill = "#ffb000",
                DisplayFont = "VT323", BodyFont = "VT323", LabelFont = "VT323", MonoFont = "VT323",
                BodyScale = 1.4, DisplayScale = 1.5, LabelScale = 1.5, Radius = "0px", LetterSpacing = ".12em",
                Glow = "0 0 18px rgba(255,176,0,.5)", OkGlow = "0 0 10px rgba(255,224,138,.5)", TextShadow = "0 0 6px rgba(255,176,0,.65)",
                Shadow = "none",
                Texture = "radial-gradient(ellipse at center, rgba(255,176,0,.07), transparent 70%)",
                TextureLayers = Layers(RadialLayer.Ellipse(0.5, 0.5, "rgba(255,176,0,.07)", "transparent 70%")),
                Overlay = "repeating-linear-gradient(0deg, rgba(0,0,0,.28) 0 1px, transparent 1px 3px), radial-gradient(ellipse at center, transparent 60%, rgba(0,0,0,.55))",
                OverlayLayers = Layers(
                    RadialLayer.Ellipse(0.5, 0.5, "transparent 60%", "rgba(0,0,0,.55)").Strength(VignetteStrength),
                    new StripeLayer(0, "rgba(0,0,0,.28)", 1, 3)),
                BadgeBorderWidth = "1px", BadgeBorderStyle = "solid", BadgeRadius = "0px", BadgeTransform = "none",
                BadgeBackground = "color-mix(in oklab, currentColor 18%, transparent)",
            });

            list.Add(new ThemeDefinition
            {
                Mode = ThemeMode.Outrun, Id = "outrun", Name = "Outrun", Tagline = "Chrome, sunset, 140 bpm.",
                Bg = "#0d0221", Panel = "#16063a", Surface = "#200b4d", Line = "#44207a", Text = "#f6ecff", Muted = "#a68ad6",
                Accent = "#ff2a6d", Ink = "#12001f", Ok = "#05d9e8", Bad = "#ffb627", Fill = "linear-gradient(90deg,#ff2a6d,#c81dff)",
                DisplayFont = "Orbitron", BodyFont = "Rajdhani", LabelFont = "Orbitron", MonoFont = "Share Tech Mono",
                BodyScale = 1.12, DisplayScale = .9, LabelScale = .92, Radius = "4px", LetterSpacing = ".2em",
                Glow = "0 0 20px rgba(255,42,109,.6), 0 0 40px rgba(200,29,255,.3)", OkGlow = "0 0 12px rgba(5,217,232,.7)",
                TextShadow = "0 0 10px rgba(255,42,109,.45)",
                Shadow = "0 0 0 1px rgba(255,42,109,.12), 0 12px 40px rgba(13,2,33,.8)",
                Texture = "linear-gradient(rgba(255,42,109,.07) 1px, transparent 1px) 0 0/44px 44px, linear-gradient(90deg, rgba(5,217,232,.06) 1px, transparent 1px) 0 0/44px 44px, radial-gradient(ellipse at 50% 120%, rgba(255,42,109,.35), transparent 60%)",
                TextureLayers = Layers(
                    RadialLayer.Ellipse(0.5, 1.2, "rgba(255,42,109,.35)", "transparent 60%"),
                    new GridLineLayer(false, "rgba(5,217,232,.06)", 1, 44),
                    new GridLineLayer(true, "rgba(255,42,109,.07)", 1, 44)),
                Overlay = "linear-gradient(180deg, rgba(255,42,109,.06), transparent 30%)",
                OverlayLayers = Layers(new LinearLayer("linear-gradient(180deg, rgba(255,42,109,.06), transparent 30%)")),
                BadgeBorderWidth = "1px", BadgeBorderStyle = "solid", BadgeRadius = "999px", BadgeTransform = "none",
                BadgeBackground = "color-mix(in oklab, currentColor 14%, transparent)",
            });

            list.Add(new ThemeDefinition
            {
                Mode = ThemeMode.PackAPunch, Id = "packapunch", Name = "Pack-a-Punch", Tagline = "Overclocked. Refined. Probably dangerous.",
                Bg = "#0b0614", Panel = "#140a25", Surface = "#1d0f36", Line = "#3f2468", Text = "#f1e8ff", Muted = "#a48cc9",
                Accent = "#c070ff", Ink = "#ffffff", Ok = "#4dffd6", Bad = "#ff4d8d", Fill = "linear-gradient(135deg,#6a1bff,#b44cff 55%,#ff4dd8)",
                DisplayFont = "Russo One", BodyFont = "Chakra Petch", LabelFont = "Russo One", MonoFont = "JetBrains Mono",
                BodyScale = 1, DisplayScale = 1, LabelScale = 1, Radius = "6px", LetterSpacing = ".16em",
                Glow = "0 0 24px rgba(180,76,255,.75), inset 0 0 12px rgba(255,255,255,.2)", OkGlow = "0 0 14px rgba(77,255,214,.6)",
                TextShadow = "0 0 14px rgba(180,76,255,.55)",
                Shadow = "0 0 0 1px rgba(180,76,255,.18), 0 0 30px rgba(106,27,255,.18)",
                Texture = "repeating-linear-gradient(115deg, rgba(180,76,255,.05) 0 1px, transparent 1px 26px), radial-gradient(ellipse at 80% 0%, rgba(180,76,255,.28), transparent 50%), radial-gradient(ellipse at 0% 100%, rgba(255,77,216,.14), transparent 45%)",
                TextureLayers = Layers(
                    RadialLayer.Ellipse(0.0, 1.0, "rgba(255,77,216,.14)", "transparent 45%"),
                    RadialLayer.Ellipse(0.8, 0.0, "rgba(180,76,255,.28)", "transparent 50%"),
                    new StripeLayer(115, "rgba(180,76,255,.05)", 1, 26)),
                Overlay = "none",
                BadgeBorderWidth = "1px", BadgeBorderStyle = "solid", BadgeRadius = "4px", BadgeTransform = "skewX(-10deg)",
                BadgeBackground = "color-mix(in oklab, currentColor 16%, transparent)",
            });

            list.Add(new ThemeDefinition
            {
                Mode = ThemeMode.NightVision, Id = "nightvision", Name = "Night Vision", Tagline = "Gen-3 tube. Everything glows. Nothing hides.",
                Bg = "#020903", Panel = "#051107", Surface = "#08190b", Line = "#1a3f1f", Text = "#b8ffb2", Muted = "#6fb86a",
                Accent = "#5dff6a", Ink = "#021003", Ok = "#5dff6a", Bad = "#f4ff7a", Fill = "#5dff6a",
                DisplayFont = "Share Tech Mono", BodyFont = "Share Tech Mono", LabelFont = "Share Tech Mono", MonoFont = "Share Tech Mono",
                BodyScale = 1.02, DisplayScale = 1.05, LabelScale = 1.05, Radius = "0px", LetterSpacing = ".2em",
                Glow = "0 0 16px rgba(93,255,106,.55)", OkGlow = "0 0 12px rgba(93,255,106,.5)", TextShadow = "0 0 8px rgba(93,255,106,.5)",
                Shadow = "inset 0 0 30px rgba(93,255,106,.04)",
                Texture = "radial-gradient(circle at 50% 40%, rgba(93,255,106,.10), transparent 60%)",
                TextureLayers = Layers(RadialLayer.Circle(0.5, 0.4, "rgba(93,255,106,.10)", "transparent 60%")),
                Overlay = "repeating-linear-gradient(0deg, rgba(0,0,0,.18) 0 1px, transparent 1px 2px), radial-gradient(circle at 50% 50%, transparent 45%, rgba(0,0,0,.8) 100%)",
                OverlayLayers = Layers(
                    RadialLayer.Circle(0.5, 0.5, "transparent 45%", "rgba(0,0,0,.8) 100%").Strength(VignetteStrength),
                    new StripeLayer(0, "rgba(0,0,0,.18)", 1, 2)),
                BadgeBorderWidth = "1px", BadgeBorderStyle = "dashed", BadgeRadius = "0px", BadgeTransform = "none", BadgeBackground = "transparent",
            });

            list.Add(new ThemeDefinition
            {
                Mode = ThemeMode.Arcade, Id = "arcade", Name = "Arcade", Tagline = "Insert coin. Eight bits of pure attitude.",
                Bg = "#11112a", Panel = "#1b1b40", Surface = "#25255a", Line = "#4a4aa8", Text = "#ffffff", Muted = "#a5a7e6",
                Accent = "#ffd400", Ink = "#11112a", Ok = "#39ff7a", Bad = "#ff3d5e", Fill = "#ffd400",
                DisplayFont = "Press Start 2P", BodyFont = "Pixelify Sans", LabelFont = "Press Start 2P", MonoFont = "Space Mono",
                BodyScale = 1.05, DisplayScale = .62, LabelScale = .72, Radius = "0px", LetterSpacing = ".06em",
                Glow = "4px 4px 0 #000", OkGlow = "3px 3px 0 #000", TextShadow = "2px 2px 0 #000", Shadow = "6px 6px 0 #000",
                Texture = "conic-gradient(rgba(255,255,255,.035) 25%, transparent 0 50%, rgba(255,255,255,.035) 0 75%, transparent 0) 0 0/24px 24px",
                TextureLayers = Layers(new CheckerLayer("rgba(255,255,255,.035)", 24)),
                Overlay = "none",
                BadgeBorderWidth = "2px", BadgeBorderStyle = "solid", BadgeRadius = "0px", BadgeTransform = "none",
                BadgeBackground = "color-mix(in oklab, currentColor 16%, transparent)",
            });

            list.Add(new ThemeDefinition
            {
                Mode = ThemeMode.Prestige, Id = "prestige", Name = "Prestige", Tagline = "Tenth prestige. Gold everything.",
                Bg = "#0b0a08", Panel = "#14120e", Surface = "#1c1914", Line = "#3d3424", Text = "#f4ecd8", Muted = "#ab9c7c",
                Accent = "#d9b44a", Ink = "#1a1406", Ok = "#a8d8a0", Bad = "#e4674f", Fill = "linear-gradient(135deg,#fbe9a0,#d9b44a 45%,#8f6b1d)",
                DisplayFont = "Cinzel", BodyFont = "Manrope", LabelFont = "Cinzel", MonoFont = "IBM Plex Mono",
                BodyScale = 1, DisplayScale = 1, LabelScale = 1, Radius = "1px", LetterSpacing = ".24em",
                Glow = "0 0 18px rgba(217,180,74,.35)", OkGlow = "none", TextShadow = "none",
                Shadow = "0 0 0 1px rgba(217,180,74,.08), 0 20px 50px rgba(0,0,0,.6)",
                Texture = "repeating-linear-gradient(90deg, rgba(217,180,74,.025) 0 1px, transparent 1px 9px), radial-gradient(ellipse at 50% -20%, rgba(217,180,74,.16), transparent 55%)",
                TextureLayers = Layers(
                    RadialLayer.Ellipse(0.5, -0.2, "rgba(217,180,74,.16)", "transparent 55%"),
                    new StripeLayer(90, "rgba(217,180,74,.025)", 1, 9)),
                Overlay = "none",
                BadgeBorderWidth = "3px", BadgeBorderStyle = "double", BadgeRadius = "1px", BadgeTransform = "none", BadgeBackground = "transparent",
            });

            // The 1.1.0 accessibility theme, kept as it was: black, white text, yellow accent.
            var contrast = new ThemeDefinition
            {
                Mode = ThemeMode.HighContrast, Id = "highcontrast", Name = "High contrast", Tagline = "Maximum legibility. No effects.",
                Bg = "#000000", Panel = "#080808", Surface = "#222222", Line = "#777777", Text = "#FFFFFF", Muted = "#E0E0E0",
                Accent = "#FFD800", Ink = "#000000", Ok = "#76FF9D", Bad = "#FF9393", Fill = "#FFD800",
                DisplayFont = "Segoe UI", BodyFont = "Segoe UI", LabelFont = "Segoe UI", MonoFont = "Consolas",
                Radius = "0px", LetterSpacing = "0",
                BadgeBorderWidth = "2px", BadgeBorderStyle = "solid", BadgeRadius = "0px",
            };
            var keys = "Accent AccentHot Ok Danger Ink Label Muted Dim Faint Ghost StatusOff Line Edge EdgeHot BtnEdge IconEdge Field Row Panel Bar Knob TagBg TagEdge DangerBg DangerEdge Divider WindowEdge".Split(' ');
            var values = new[] {
                "#FFD800", "#FFF06A", "#76FF9D", "#FF9393", "#FFFFFF", "#F3F3F3", "#E0E0E0", "#CCCCCC", "#BBBBBB", "#AAAAAA", "#AAAAAA",
                "#777777", "#AAAAAA", "#FFFFFF", "#AAAAAA", "#AAAAAA", "#111111", "#222222", "#000000", "#080808", "#000000", "#292400", "#FFD800", "#300000", "#FF9393", "#AAAAAA", "#FFFFFF" };
            for (int i = 0; i < keys.Length; i++) contrast.Overrides[keys[i]] = values[i];
            contrast.Overrides["Surface"] = "#222222";
            contrast.Overrides["FillInk"] = "#000000";
            list.Add(contrast);

            return list;
        }

        private static IList<ThemeLayer> Layers(params ThemeLayer[] layers) { return new List<ThemeLayer>(layers); }
    }
}
