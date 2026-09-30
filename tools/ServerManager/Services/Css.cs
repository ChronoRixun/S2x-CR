using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Effects;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// The handful of CSS value forms the theme pack's tokens are written in, turned into WPF
    /// objects: colours, linear-gradient fills, box/text shadows, transforms and the
    /// color-mix() badge tints. Everything returned is frozen, so it can be shared between
    /// elements and threads.
    /// </summary>
    public static class Css
    {
        private static readonly CultureInfo Invariant = CultureInfo.InvariantCulture;

        // ── colours ───────────────────────────────────────────────────────────────
        /// <summary>#rgb, #rrggbb, #aarrggbb, rgb(), rgba() and transparent.</summary>
        public static Color ParseColor(string text)
        {
            var value = (text ?? "").Trim();
            if (value.Length == 0) throw new FormatException("Empty colour.");
            if (string.Equals(value, "transparent", StringComparison.OrdinalIgnoreCase)) return Colors.Transparent;
            if (value.StartsWith("rgb", StringComparison.OrdinalIgnoreCase))
            {
                var open = value.IndexOf('(');
                var close = value.LastIndexOf(')');
                if (open < 0 || close < open) throw new FormatException("Bad colour: " + value);
                var parts = value.Substring(open + 1, close - open - 1).Split(',');
                if (parts.Length < 3) throw new FormatException("Bad colour: " + value);
                var alpha = parts.Length > 3 ? Number(parts[3]) : 1.0;
                return System.Windows.Media.Color.FromArgb(Byte(alpha * 255), Byte(Number(parts[0])), Byte(Number(parts[1])), Byte(Number(parts[2])));
            }
            if (value[0] == '#' && value.Length == 4)
                value = "#" + value[1] + value[1] + value[2] + value[2] + value[3] + value[3];
            return (Color)ColorConverter.ConvertFromString(value);
        }

        /// <summary>A frozen solid brush for a CSS colour.</summary>
        public static SolidColorBrush Solid(Color color)
        {
            var brush = new SolidColorBrush(color);
            brush.Freeze();
            return brush;
        }

        // ── fills ────────────────────────────────────────────────────────────────
        /// <summary>A solid colour or a linear-gradient(): the pack's "fill" token.</summary>
        public static Brush Fill(string text)
        {
            var value = (text ?? "").Trim();
            if (value.StartsWith("linear-gradient", StringComparison.OrdinalIgnoreCase)) return LinearGradient(value);
            return Solid(ParseColor(value));
        }

        /// <summary>
        /// linear-gradient(&lt;angle&gt;, stop, stop [n%], ...). CSS measures the angle clockwise
        /// from "to top" and sizes the gradient line so the corners get the end colours. This maps
        /// it into the element's unit box (RelativeToBoundingBox): exact for 0/90/180/270 and for
        /// square boxes; a diagonal on a wide box runs corner to corner, as "to bottom right" does.
        /// </summary>
        public static LinearGradientBrush LinearGradient(string text)
        {
            var args = Arguments(text);
            var angle = 180.0;   // CSS default: to bottom
            var first = 0;
            if (args.Count > 0 && args[0].EndsWith("deg", StringComparison.OrdinalIgnoreCase))
            {
                angle = Number(args[0].Substring(0, args[0].Length - 3));
                first = 1;
            }
            Point start, end;
            AnglePoints(angle, out start, out end);
            var brush = new LinearGradientBrush { StartPoint = start, EndPoint = end, MappingMode = BrushMappingMode.RelativeToBoundingBox };
            foreach (var stop in Stops(args, first)) brush.GradientStops.Add(stop);
            brush.Freeze();
            return brush;
        }

        /// <summary>The unit-box start and end points of a CSS gradient angle.</summary>
        public static void AnglePoints(double degrees, out Point start, out Point end)
        {
            var radians = degrees * Math.PI / 180.0;
            // Direction in screen space (y grows downwards): 0deg points up, 90deg right.
            var dx = Math.Sin(radians);
            var dy = -Math.Cos(radians);
            // Half the gradient line in a unit square, so the corners land on 0 and 1.
            var half = (Math.Abs(dx) + Math.Abs(dy)) / 2.0;
            start = new Point(Round(0.5 - dx * half), Round(0.5 - dy * half));
            end = new Point(Round(0.5 + dx * half), Round(0.5 + dy * half));
        }

        /// <summary>
        /// Colour stops with CSS positioning: missing positions are spread evenly between the
        /// known ones, the first defaults to 0% and the last to 100%. "transparent" takes its
        /// neighbour's colour at zero alpha, because WPF interpolates without premultiplying and
        /// fading to transparent black would grey the colour on the way.
        /// </summary>
        internal static List<GradientStop> Stops(IList<string> args, int first)
        {
            var colors = new List<Color>();
            var transparent = new List<bool>();
            var positions = new List<double?>();
            for (int i = first; i < args.Count; i++)
            {
                var part = args[i].Trim();
                double? position = null;
                var space = LastTopLevelSpace(part);
                if (space > 0 && part.EndsWith("%", StringComparison.Ordinal))
                {
                    position = Number(part.Substring(space + 1, part.Length - space - 2)) / 100.0;
                    part = part.Substring(0, space).Trim();
                }
                transparent.Add(string.Equals(part, "transparent", StringComparison.OrdinalIgnoreCase));
                colors.Add(ParseColor(part));
                positions.Add(position);
            }
            if (colors.Count == 0) throw new FormatException("A gradient needs colour stops.");
            if (!positions[0].HasValue) positions[0] = 0;
            if (!positions[positions.Count - 1].HasValue) positions[positions.Count - 1] = 1;
            for (int i = 1; i < positions.Count - 1; i++)
            {
                if (positions[i].HasValue) continue;
                int next = i;
                while (!positions[next].HasValue) next++;
                var from = positions[i - 1].Value;
                var to = positions[next].Value;
                for (int j = i; j < next; j++) positions[j] = from + (to - from) * (j - i + 1) / (next - i + 1);
            }
            for (int i = 0; i < colors.Count; i++)
            {
                if (!transparent[i]) continue;
                var neighbour = i > 0 && !transparent[i - 1] ? colors[i - 1] : i + 1 < colors.Count ? colors[i + 1] : colors[i];
                colors[i] = System.Windows.Media.Color.FromArgb(0, neighbour.R, neighbour.G, neighbour.B);
            }
            var stops = new List<GradientStop>();
            for (int i = 0; i < colors.Count; i++) stops.Add(new GradientStop(colors[i], positions[i].Value));
            return stops;
        }

        /// <summary>The same fill a little brighter, like CSS filter: brightness(1.15) on hover.</summary>
        public static Brush Brighter(Brush fill, double factor)
        {
            var solid = fill as SolidColorBrush;
            if (solid != null) return Solid(Scale(solid.Color, factor));
            var gradient = fill as LinearGradientBrush;
            if (gradient == null) return fill;
            var copy = gradient.Clone();
            foreach (var stop in copy.GradientStops) stop.Color = Scale(stop.Color, factor);
            copy.Freeze();
            return copy;
        }

        private static Color Scale(Color color, double factor)
        {
            return System.Windows.Media.Color.FromArgb(color.A, Byte(color.R * factor), Byte(color.G * factor), Byte(color.B * factor));
        }

        // ── shadows ──────────────────────────────────────────────────────────────
        /// <summary>One layer of a box-shadow or text-shadow list.</summary>
        public sealed class ShadowLayer
        {
            public double X, Y, Blur, Spread;
            public Color Color;
            public bool Inset;
        }

        public static List<ShadowLayer> ShadowLayers(string text)
        {
            var layers = new List<ShadowLayer>();
            var value = (text ?? "").Trim();
            if (value.Length == 0 || string.Equals(value, "none", StringComparison.OrdinalIgnoreCase)) return layers;
            foreach (var raw in SplitTopLevel(value, ','))
            {
                var layer = new ShadowLayer { Color = Colors.Black };
                var lengths = new List<double>();
                foreach (var token in SplitTopLevel(raw.Trim(), ' '))
                {
                    var part = token.Trim();
                    if (part.Length == 0) continue;
                    if (string.Equals(part, "inset", StringComparison.OrdinalIgnoreCase)) { layer.Inset = true; continue; }
                    if (part[0] == '#' || part.StartsWith("rgb", StringComparison.OrdinalIgnoreCase) || char.IsLetter(part[0]))
                    { layer.Color = ParseColor(part); continue; }
                    lengths.Add(Px(part));
                }
                if (lengths.Count > 0) layer.X = lengths[0];
                if (lengths.Count > 1) layer.Y = lengths[1];
                if (lengths.Count > 2) layer.Blur = lengths[2];
                if (lengths.Count > 3) layer.Spread = lengths[3];
                layers.Add(layer);
            }
            return layers;
        }

        /// <summary>
        /// A box-shadow or text-shadow as a DropShadowEffect, or null for none. WPF draws one
        /// shadow per element, so this takes the first outer layer that casts one: inset layers and
        /// spread-only rings (0 0 0 1px) have no DropShadowEffect equivalent and are skipped. A glow
        /// (no offset) becomes ShadowDepth 0; a hard offset shadow (4px 4px 0 #000) keeps its
        /// direction and distance with BlurRadius 0.
        /// </summary>
        public static DropShadowEffect Shadow(string text)
        {
            foreach (var layer in ShadowLayers(text))
            {
                if (layer.Inset) continue;
                if (layer.X == 0 && layer.Y == 0 && layer.Blur == 0) continue;
                var effect = new DropShadowEffect
                {
                    Color = System.Windows.Media.Color.FromRgb(layer.Color.R, layer.Color.G, layer.Color.B),
                    Opacity = layer.Color.A / 255.0,
                    BlurRadius = layer.Blur,
                    ShadowDepth = Math.Sqrt(layer.X * layer.X + layer.Y * layer.Y),
                    // WPF measures the direction anticlockwise from "right" with y up; CSS offsets
                    // are y down.
                    Direction = Normalize(Math.Atan2(-layer.Y, layer.X) * 180.0 / Math.PI),
                    RenderingBias = RenderingBias.Performance,
                };
                effect.Freeze();
                return effect;
            }
            return null;
        }

        // ── transforms and badge values ──────────────────────────────────────────
        /// <summary>none, rotate(&lt;n&gt;deg) or skewX(&lt;n&gt;deg), about the element's centre.</summary>
        public static Transform ParseTransform(string text)
        {
            var value = (text ?? "").Trim();
            Transform result = System.Windows.Media.Transform.Identity;
            var match = Regex.Match(value, @"^(rotate|skewx|skewy)\(\s*(-?[0-9.]+)deg\s*\)$", RegexOptions.IgnoreCase);
            if (match.Success)
            {
                var angle = Number(match.Groups[2].Value);
                switch (match.Groups[1].Value.ToLowerInvariant())
                {
                    case "rotate": result = new RotateTransform(angle); break;
                    case "skewx": result = new SkewTransform(angle, 0); break;
                    default: result = new SkewTransform(0, angle); break;
                }
                result.Freeze();
            }
            return result;
        }

        /// <summary>
        /// color-mix(in oklab, currentColor N%, transparent) is the status colour at N% alpha;
        /// "transparent" is none. Returns the alpha as 0-1.
        /// </summary>
        public static double MixAlpha(string text)
        {
            var match = Regex.Match(text ?? "", @"currentColor\s+([0-9.]+)%", RegexOptions.IgnoreCase);
            return match.Success ? Number(match.Groups[1].Value) / 100.0 : 0.0;
        }

        /// <summary>"2px" → 2, "999px" → 999, "0" → 0, ".16em" → 0.16.</summary>
        public static double Px(string text)
        {
            var value = (text ?? "").Trim();
            if (value.EndsWith("px", StringComparison.OrdinalIgnoreCase)) value = value.Substring(0, value.Length - 2);
            else if (value.EndsWith("em", StringComparison.OrdinalIgnoreCase)) value = value.Substring(0, value.Length - 2);
            return value.Length == 0 ? 0 : Number(value);
        }

        // ── parsing helpers ──────────────────────────────────────────────────────
        internal static List<string> Arguments(string function)
        {
            var open = function.IndexOf('(');
            var close = function.LastIndexOf(')');
            if (open < 0 || close < open) throw new FormatException("Bad CSS function: " + function);
            var list = new List<string>();
            foreach (var part in SplitTopLevel(function.Substring(open + 1, close - open - 1), ',')) list.Add(part.Trim());
            return list;
        }

        /// <summary>Splits on a separator outside parentheses: rgba(0,0,0,.5) stays whole.</summary>
        internal static List<string> SplitTopLevel(string text, char separator)
        {
            var parts = new List<string>();
            int depth = 0, start = 0;
            for (int i = 0; i < text.Length; i++)
            {
                if (text[i] == '(') depth++;
                else if (text[i] == ')') depth--;
                else if (text[i] == separator && depth == 0)
                {
                    parts.Add(text.Substring(start, i - start));
                    start = i + 1;
                }
            }
            parts.Add(text.Substring(start));
            parts.RemoveAll(p => p.Trim().Length == 0);
            return parts;
        }

        private static int LastTopLevelSpace(string text)
        {
            int depth = 0, last = -1;
            for (int i = 0; i < text.Length; i++)
            {
                if (text[i] == '(') depth++;
                else if (text[i] == ')') depth--;
                else if (text[i] == ' ' && depth == 0) last = i;
            }
            return last;
        }

        private static double Number(string text) { return double.Parse(text.Trim(), NumberStyles.Float, Invariant); }
        private static byte Byte(double value) { return (byte)Math.Max(0, Math.Min(255, Math.Round(value))); }
        private static double Round(double value) { return Math.Round(value, 6); }
        private static double Normalize(double degrees) { degrees %= 360; return degrees < 0 ? degrees + 360 : degrees; }
    }
}
