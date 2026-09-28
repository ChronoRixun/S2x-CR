using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Resources;
using System.Windows;
using System.Windows.Media;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// One family the theme pack asks for: the file it comes in, where that file is published,
    /// and what stands in for it on a machine where the file was not built into the exe.
    /// </summary>
    public sealed class FontSpec
    {
        public string Family { get; set; }
        /// <summary>
        /// Files to drop into tools/ServerManager/Assets/Fonts (any file carrying the family works).
        /// No square brackets: google/fonts names variable fonts Family[wght].ttf, and WPF cannot
        /// load a pack resource with [ ] in its name, so those are saved as Family-VariableFont_wght.ttf.
        /// </summary>
        public string[] Files { get; set; }
        /// <summary>Path in github.com/google/fonts.</summary>
        public string Source { get; set; }
        public string License { get; set; }
        /// <summary>Installed Windows families, first one present wins.</summary>
        public string Fallback { get; set; }
        /// <summary>Weight for display and label text drawn in the fallback.</summary>
        public FontWeight FallbackWeight { get; set; }
        public FontStretch FallbackStretch { get; set; }
        /// <summary>
        /// Multiplies the theme's font scale when the fallback is drawn. The pack's scales make up
        /// for its own fonts' metrics (VT323 is small, Press Start 2P is huge); a stand-in with
        /// ordinary metrics needs that taken back out.
        /// </summary>
        public double FallbackScale { get; set; }
        public bool Mono { get; set; }
    }

    /// <summary>What a role (display, body, label, mono) is drawn in for the current machine.</summary>
    public sealed class ResolvedFont
    {
        public FontFamily Family { get; set; }
        public FontWeight Weight { get; set; }
        public FontStretch Stretch { get; set; }
        /// <summary>1 for the embedded family, the spec's FallbackScale otherwise.</summary>
        public double Scale { get; set; }
        public bool Embedded { get; set; }
        /// <summary>The family actually used, for the README table and tests.</summary>
        public string Used { get; set; }
    }

    /// <summary>
    /// Font roles. The exe stays single: design fonts are WPF Resources built from
    /// Assets/Fonts (see Assets/Fonts/README.md). A family found there is used; otherwise the
    /// spec's installed Windows fallback is. Adding the files later needs no code change.
    /// </summary>
    public static class ThemeFonts
    {
        private const string Folder = "assets/fonts/";
        private static HashSet<string> _embedded;
        private static HashSet<string> _installed;
        private static readonly Dictionary<string, ResolvedFont> Cache = new Dictionary<string, ResolvedFont>(StringComparer.OrdinalIgnoreCase);

        private static readonly FontWeight Normal = FontWeights.Normal;
        private static readonly FontStretch Wide = FontStretches.Normal;

        /// <summary>Every family the pack names, in the order the design lists them.</summary>
        public static readonly IList<FontSpec> Specs = new List<FontSpec>
        {
            Spec("Black Ops One", "ofl/blackopsone", "OFL-1.1", "Bahnschrift", FontWeights.Bold, Wide, 1.0, "BlackOpsOne-Regular.ttf"),
            Spec("Barlow Condensed", "ofl/barlowcondensed", "OFL-1.1", "Bahnschrift", FontWeights.SemiBold, FontStretches.Condensed, 0.95,
                 "BarlowCondensed-Regular.ttf", "BarlowCondensed-SemiBold.ttf", "BarlowCondensed-Bold.ttf"),
            Spec("Special Elite", "apache/specialelite", "Apache-2.0", "Courier New", FontWeights.Bold, Wide, 1.0, "SpecialElite-Regular.ttf").AsMono(),
            Spec("Creepster", "ofl/creepster", "OFL-1.1", "Impact", Normal, Wide, 0.9, "Creepster-Regular.ttf"),
            Spec("Oswald", "ofl/oswald", "OFL-1.1", "Bahnschrift", FontWeights.SemiBold, FontStretches.SemiCondensed, 1.0, "Oswald-VariableFont_wght.ttf"),
            Spec("JetBrains Mono", "ofl/jetbrainsmono", "OFL-1.1", "Cascadia Mono, Consolas", Normal, Wide, 1.0, "JetBrainsMono-VariableFont_wght.ttf").AsMono(),
            Spec("VT323", "ofl/vt323", "OFL-1.1", "Consolas", Normal, Wide, 0.72, "VT323-Regular.ttf").AsMono(),
            Spec("Orbitron", "ofl/orbitron", "OFL-1.1", "Bahnschrift", FontWeights.SemiBold, Wide, 1.1, "Orbitron-VariableFont_wght.ttf"),
            Spec("Rajdhani", "ofl/rajdhani", "OFL-1.1", "Bahnschrift", FontWeights.SemiBold, FontStretches.SemiCondensed, 0.93,
                 "Rajdhani-Medium.ttf", "Rajdhani-Bold.ttf"),
            Spec("Share Tech Mono", "ofl/sharetechmono", "OFL-1.1", "Consolas", Normal, Wide, 0.95, "ShareTechMono-Regular.ttf").AsMono(),
            Spec("Russo One", "ofl/russoone", "OFL-1.1", "Bahnschrift", FontWeights.Bold, Wide, 1.0, "RussoOne-Regular.ttf"),
            Spec("Chakra Petch", "ofl/chakrapetch", "OFL-1.1", "Bahnschrift", FontWeights.SemiBold, Wide, 1.0,
                 "ChakraPetch-Regular.ttf", "ChakraPetch-SemiBold.ttf"),
            Spec("Press Start 2P", "ofl/pressstart2p", "OFL-1.1", "Lucida Console", Normal, Wide, 1.45, "PressStart2P-Regular.ttf"),
            Spec("Pixelify Sans", "ofl/pixelifysans", "OFL-1.1", "Bahnschrift", FontWeights.SemiBold, Wide, 0.97, "PixelifySans-VariableFont_wght.ttf"),
            Spec("Space Mono", "ofl/spacemono", "OFL-1.1", "Cascadia Mono, Consolas", Normal, Wide, 0.97,
                 "SpaceMono-Regular.ttf", "SpaceMono-Bold.ttf").AsMono(),
            Spec("Cinzel", "ofl/cinzel", "OFL-1.1", "Palatino Linotype, Georgia", Normal, Wide, 1.0, "Cinzel-VariableFont_wght.ttf"),
            Spec("Manrope", "ofl/manrope", "OFL-1.1", "Segoe UI", FontWeights.SemiBold, Wide, 1.0, "Manrope-VariableFont_wght.ttf"),
            Spec("IBM Plex Mono", "ofl/ibmplexmono", "OFL-1.1", "Cascadia Mono, Consolas", Normal, Wide, 1.0, "IBMPlexMono-Regular.ttf").AsMono(),
            // High contrast keeps the app's own system fonts; nothing to embed.
            Spec("Segoe UI", null, "system", "Segoe UI", FontWeights.SemiBold, Wide, 1.0),
            Spec("Consolas", null, "system", "Consolas", Normal, Wide, 1.0).AsMono(),
        };

        private static FontSpec Spec(string family, string source, string license, string fallback, FontWeight weight,
                                     FontStretch stretch, double scale, params string[] files)
        {
            return new FontSpec
            {
                Family = family, Source = source, License = license, Fallback = fallback, FallbackWeight = weight,
                FallbackStretch = stretch, FallbackScale = scale, Files = files ?? new string[0],
            };
        }

        private static FontSpec AsMono(this FontSpec spec) { spec.Mono = true; return spec; }

        public static FontSpec Find(string family)
        {
            return Specs.FirstOrDefault(s => string.Equals(s.Family, Unquote(family), StringComparison.OrdinalIgnoreCase));
        }

        /// <summary>Families built into this exe from Assets/Fonts, read once.</summary>
        public static ICollection<string> EmbeddedFamilies
        {
            get
            {
                if (_embedded == null) _embedded = ReadEmbedded();
                return _embedded;
            }
        }

        /// <summary>
        /// The role's family: embedded when its file is in the assembly, else the spec's first
        /// installed fallback. Display and label roles take the fallback's weight and stretch; body
        /// and mono text keep normal weight so the views' own SemiBold and Bold still mean something.
        /// </summary>
        public static ResolvedFont Resolve(string design, bool strongRole)
        {
            var family = Unquote(design);
            var key = family + (strongRole ? "|strong" : "|text");
            ResolvedFont resolved;
            if (Cache.TryGetValue(key, out resolved)) return resolved;

            var spec = Find(family) ?? Spec(family, null, "unknown", "Segoe UI", Normal, Wide, 1.0);
            var generic = spec.Mono ? "Consolas" : "Segoe UI";
            if (spec.Files.Length > 0 && EmbeddedFamilies.Contains(spec.Family))
            {
                resolved = new ResolvedFont
                {
                    Family = new FontFamily(BaseUri, "./Assets/Fonts/#" + spec.Family + ", " + spec.Fallback + ", " + generic + ", Segoe UI Symbol"),
                    Weight = FontWeights.Normal,
                    Stretch = FontStretches.Normal,
                    Scale = 1.0,
                    Embedded = true,
                    Used = spec.Family + " (embedded)",
                };
            }
            else
            {
                var chain = spec.Fallback.Split(',').Select(f => f.Trim()).ToList();
                var first = chain.FirstOrDefault(Installed) ?? generic;
                resolved = new ResolvedFont
                {
                    Family = new FontFamily(first + ", " + string.Join(", ", chain.Where(f => f != first)) + (chain.Count > 1 ? ", " : "") + generic + ", Segoe UI Symbol"),
                    Weight = strongRole ? spec.FallbackWeight : FontWeights.Normal,
                    Stretch = spec.FallbackStretch,
                    Scale = spec.FallbackScale,
                    Embedded = false,
                    Used = first + Describe(strongRole ? spec.FallbackWeight : FontWeights.Normal, spec.FallbackStretch),
                };
            }
            Cache[key] = resolved;
            return resolved;
        }

        private static string Describe(FontWeight weight, FontStretch stretch)
        {
            var parts = new List<string>();
            if (weight != FontWeights.Normal) parts.Add(weight.ToString());
            if (stretch != FontStretches.Normal) parts.Add(stretch.ToString());
            return parts.Count == 0 ? "" : " " + string.Join(" ", parts);
        }

        private static bool Installed(string family)
        {
            if (_installed == null)
            {
                _installed = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach (var installed in Fonts.SystemFontFamilies)
                    foreach (var name in installed.FamilyNames.Values) _installed.Add(name);
            }
            return _installed.Contains(family);
        }

        private static Uri BaseUri
        {
            get { return new Uri("pack://application:,,,/" + typeof(ThemeFonts).Assembly.GetName().Name + ";component/"); }
        }

        /// <summary>
        /// Family names of every .ttf/.otf compiled in as a WPF Resource under Assets/Fonts.
        /// A file that cannot be read is skipped: its family then falls back like a missing one.
        /// </summary>
        private static HashSet<string> ReadEmbedded()
        {
            var families = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var key in EmbeddedFontFiles())
            {
                try
                {
                    var face = new GlyphTypeface(new Uri(BaseUri, key));
                    foreach (var name in face.FamilyNames.Values) families.Add(name);
                    foreach (var name in face.Win32FamilyNames.Values) families.Add(name);
                }
                catch (Exception ex) when (ex is IOException || ex is FileFormatException || ex is UriFormatException || ex is NotSupportedException || ex is ArgumentException)
                {
                }
            }
            return families;
        }

        /// <summary>The resource keys (lower-case paths) of the font files in the assembly.</summary>
        public static IList<string> EmbeddedFontFiles()
        {
            var files = new List<string>();
            var assembly = typeof(ThemeFonts).Assembly;
            using (var stream = assembly.GetManifestResourceStream(assembly.GetName().Name + ".g.resources"))
            {
                if (stream == null) return files;
                using (var reader = new ResourceReader(stream))
                {
                    foreach (DictionaryEntry entry in reader)
                    {
                        var key = entry.Key as string;
                        if (key == null || !key.StartsWith(Folder, StringComparison.OrdinalIgnoreCase)) continue;
                        if (key.EndsWith(".ttf", StringComparison.OrdinalIgnoreCase) || key.EndsWith(".otf", StringComparison.OrdinalIgnoreCase))
                            files.Add(Uri.UnescapeDataString(key));
                    }
                }
            }
            return files;
        }

        private static string Unquote(string family)
        {
            return (family ?? "").Trim().Trim('\'', '"');
        }
    }
}
