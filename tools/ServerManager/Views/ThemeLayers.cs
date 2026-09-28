using System;
using System.Windows;
using System.Windows.Media;
using S2x.ServerManager.Services;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// One layer of a theme's background texture or overlay, the WPF form of one CSS
    /// background layer. Tiled layers build one frozen brush and keep it; radial layers depend
    /// on the size they cover (CSS sizes them to the farthest corner) and are built per render,
    /// which only happens on a resize or a theme change.
    /// </summary>
    public abstract class ThemeLayer
    {
        public abstract Brush Create(Size size);

        protected static Brush Freeze(Brush brush)
        {
            brush.Freeze();
            return brush;
        }

        /// <summary>A tile of <paramref name="width"/> x <paramref name="height"/> px with some rectangles painted.</summary>
        protected static DrawingBrush Tile(double width, double height, Color color, params Rect[] painted)
        {
            var group = new DrawingGroup();
            // An invisible rectangle pins the tile's bounds: a drawing is only as big as its ink.
            group.Children.Add(new GeometryDrawing(Brushes.Transparent, null, new RectangleGeometry(new Rect(0, 0, width, height))));
            var ink = new SolidColorBrush(color);
            var geometry = new GeometryGroup();
            foreach (var rect in painted) geometry.Children.Add(new RectangleGeometry(rect));
            group.Children.Add(new GeometryDrawing(ink, null, geometry));
            var brush = new DrawingBrush(group)
            {
                TileMode = TileMode.Tile,
                Stretch = Stretch.None,
                AlignmentX = AlignmentX.Left,
                AlignmentY = AlignmentY.Top,
                Viewbox = new Rect(0, 0, width, height),
                ViewboxUnits = BrushMappingMode.Absolute,
                Viewport = new Rect(0, 0, width, height),
                ViewportUnits = BrushMappingMode.Absolute,
            };
            RenderOptions.SetCachingHint(brush, CachingHint.Cache);
            return brush;
        }
    }

    /// <summary>
    /// repeating-linear-gradient(&lt;angle&gt;, colour 0 &lt;stripe&gt;px, transparent &lt;stripe&gt;px &lt;period&gt;px):
    /// parallel stripes. The CSS angle is the direction the colour changes in, so the stripes run
    /// across it: 90deg is vertical lines, 0deg horizontal (scanlines), 45deg leans like "\".
    /// </summary>
    public sealed class StripeLayer : ThemeLayer
    {
        private readonly double _angle, _stripe, _period;
        private readonly Color _color;
        private Brush _brush;

        public StripeLayer(double cssAngle, string color, double stripe, double period)
        {
            _angle = cssAngle; _color = Css.ParseColor(color); _stripe = stripe; _period = period;
        }

        public override Brush Create(Size size)
        {
            if (_brush != null) return _brush;
            var turn = ((_angle % 180) + 180) % 180;
            DrawingBrush tile;
            if (turn == 0) tile = Tile(_period, _period, _color, new Rect(0, _period - _stripe, _period, _stripe));
            else
            {
                tile = Tile(_period, _period, _color, new Rect(0, 0, _stripe, _period));
                // Vertical stripes are the 90deg case; any other angle turns them about the origin.
                if (turn != 90) tile.Transform = new RotateTransform(_angle - 90);
            }
            return _brush = Freeze(tile);
        }
    }

    /// <summary>linear-gradient(colour 1px, transparent 1px) 0 0/44px 44px: one rule per cell.</summary>
    public sealed class GridLineLayer : ThemeLayer
    {
        private readonly bool _horizontal;
        private readonly Color _color;
        private readonly double _line, _cell;
        private Brush _brush;

        public GridLineLayer(bool horizontal, string color, double line, double cell)
        {
            _horizontal = horizontal; _color = Css.ParseColor(color); _line = line; _cell = cell;
        }

        public override Brush Create(Size size)
        {
            if (_brush != null) return _brush;
            var rule = _horizontal ? new Rect(0, 0, _cell, _line) : new Rect(0, 0, _line, _cell);
            return _brush = Freeze(Tile(_cell, _cell, _color, rule));
        }
    }

    /// <summary>
    /// conic-gradient(c 25%, transparent 0 50%, c 0 75%, transparent 0) 0 0/24px 24px: a
    /// checkerboard of half-tile squares, top-right and bottom-left painted.
    /// </summary>
    public sealed class CheckerLayer : ThemeLayer
    {
        private readonly Color _color;
        private readonly double _tile;
        private Brush _brush;

        public CheckerLayer(string color, double tile)
        {
            _color = Css.ParseColor(color); _tile = tile;
        }

        public override Brush Create(Size size)
        {
            if (_brush != null) return _brush;
            var half = _tile / 2;
            return _brush = Freeze(Tile(_tile, _tile, _color, new Rect(half, 0, half, half), new Rect(0, half, half, half)));
        }
    }

    /// <summary>repeating-radial-gradient(circle at x y, colour 0 Npx, transparent Npx Ppx): rings.</summary>
    public sealed class RingLayer : ThemeLayer
    {
        private readonly double _cx, _cy, _ring, _period;
        private readonly Color _color;

        public RingLayer(double cx, double cy, string color, double ring, double period)
        {
            _cx = cx; _cy = cy; _color = Css.ParseColor(color); _ring = ring; _period = period;
        }

        public override Brush Create(Size size)
        {
            var center = new Point(_cx * size.Width, _cy * size.Height);
            var clear = Color.FromArgb(0, _color.R, _color.G, _color.B);
            var edge = _ring / _period;
            var brush = new RadialGradientBrush
            {
                MappingMode = BrushMappingMode.Absolute,
                Center = center, GradientOrigin = center,
                RadiusX = _period, RadiusY = _period,
                SpreadMethod = GradientSpreadMethod.Repeat,
            };
            brush.GradientStops.Add(new GradientStop(_color, 0));
            brush.GradientStops.Add(new GradientStop(_color, edge));
            brush.GradientStops.Add(new GradientStop(clear, edge));
            brush.GradientStops.Add(new GradientStop(clear, 1));
            return Freeze(brush);
        }
    }

    /// <summary>
    /// radial-gradient(ellipse|circle at x y, stops): sized to the farthest corner, the CSS
    /// default. A circle's radius is the distance to that corner; an ellipse keeps the aspect of
    /// the farthest sides and is scaled by root 2 to pass through the corner.
    /// </summary>
    public sealed class RadialLayer : ThemeLayer
    {
        private readonly double _cx, _cy;
        private readonly bool _circle;
        private readonly string[] _stops;
        private double _strength = 1;

        private RadialLayer(double cx, double cy, bool circle, string[] stops)
        {
            _cx = cx; _cy = cy; _circle = circle; _stops = stops;
            Css.Stops(stops, 0);   // fail at startup, not at first paint, on a bad stop
        }

        public static RadialLayer Ellipse(double cx, double cy, params string[] stops) { return new RadialLayer(cx, cy, false, stops); }
        public static RadialLayer Circle(double cx, double cy, params string[] stops) { return new RadialLayer(cx, cy, true, stops); }

        /// <summary>Scales every stop's alpha: a vignette drawn over a working window's corners.</summary>
        public RadialLayer Strength(double factor)
        {
            _strength = factor;
            return this;
        }

        public override Brush Create(Size size)
        {
            var x = _cx * size.Width;
            var y = _cy * size.Height;
            var dx = Math.Max(Math.Abs(x), Math.Abs(size.Width - x));
            var dy = Math.Max(Math.Abs(y), Math.Abs(size.Height - y));
            double rx, ry;
            if (_circle) rx = ry = Math.Sqrt(dx * dx + dy * dy);
            else { rx = dx * Math.Sqrt(2); ry = dy * Math.Sqrt(2); }
            var brush = new RadialGradientBrush
            {
                MappingMode = BrushMappingMode.Absolute,
                Center = new Point(x, y), GradientOrigin = new Point(x, y),
                RadiusX = Math.Max(1, rx), RadiusY = Math.Max(1, ry),
            };
            foreach (var stop in Css.Stops(_stops, 0))
            {
                if (_strength != 1) stop.Color = Color.FromArgb((byte)Math.Round(stop.Color.A * _strength), stop.Color.R, stop.Color.G, stop.Color.B);
                brush.GradientStops.Add(stop);
            }
            return Freeze(brush);
        }
    }

    /// <summary>A plain linear-gradient() over the whole area (Outrun's top wash).</summary>
    public sealed class LinearLayer : ThemeLayer
    {
        private readonly Brush _brush;

        public LinearLayer(string css) { _brush = Css.LinearGradient(css); }

        public override Brush Create(Size size) { return _brush; }
    }
}
