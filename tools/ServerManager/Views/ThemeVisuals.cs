using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using S2x.ServerManager.Services;

namespace S2x.ServerManager.Views
{
    /// <summary>Attached properties the themed templates read.</summary>
    public static class ThemeAssist
    {
        /// <summary>Corner radius for a button or box template (the theme's Radius, or 0 where it sits flush in a bar).</summary>
        public static readonly DependencyProperty CornerRadiusProperty =
            DependencyProperty.RegisterAttached("CornerRadius", typeof(CornerRadius), typeof(ThemeAssist),
                new FrameworkPropertyMetadata(new CornerRadius(0), FrameworkPropertyMetadataOptions.AffectsRender));
        public static void SetCornerRadius(DependencyObject target, CornerRadius value) { target.SetValue(CornerRadiusProperty, value); }
        public static CornerRadius GetCornerRadius(DependencyObject target) { return (CornerRadius)target.GetValue(CornerRadiusProperty); }

        /// <summary>The selected button of a segmented control: surface, accent text and an accent underline.</summary>
        public static readonly DependencyProperty ActiveProperty =
            DependencyProperty.RegisterAttached("Active", typeof(bool), typeof(ThemeAssist), new FrameworkPropertyMetadata(false));
        public static void SetActive(DependencyObject target, bool value) { target.SetValue(ActiveProperty, value); }
        public static bool GetActive(DependencyObject target) { return (bool)target.GetValue(ActiveProperty); }

        /// <summary>
        /// Clips an element to a rounded rectangle of this radius, so square children (a filled
        /// button in a card's corner) do not poke out of a rounded border. 0 clears the clip.
        /// </summary>
        public static readonly DependencyProperty ClipRadiusProperty =
            DependencyProperty.RegisterAttached("ClipRadius", typeof(double), typeof(ThemeAssist),
                new PropertyMetadata(0.0, OnClipRadiusChanged));
        public static void SetClipRadius(DependencyObject target, double value) { target.SetValue(ClipRadiusProperty, value); }
        public static double GetClipRadius(DependencyObject target) { return (double)target.GetValue(ClipRadiusProperty); }

        private static void OnClipRadiusChanged(DependencyObject target, DependencyPropertyChangedEventArgs e)
        {
            var element = target as FrameworkElement;
            if (element == null) return;
            element.SizeChanged -= Reclip;
            element.SizeChanged += Reclip;
            Clip(element);
        }

        private static void Reclip(object sender, SizeChangedEventArgs e) { Clip((FrameworkElement)sender); }

        private static void Clip(FrameworkElement element)
        {
            var radius = Fit(GetClipRadius(element), element);
            if (radius <= 0 || element.ActualWidth <= 0) { element.Clip = null; return; }
            var clip = new RectangleGeometry(new Rect(0, 0, element.ActualWidth, element.ActualHeight), radius, radius);
            clip.Freeze();
            element.Clip = clip;
        }

        /// <summary>
        /// A corner radius the way CSS applies one: never more than half the shorter side, the
        /// same on both axes. WPF clamps a Border's X and Y radii separately, so Outrun's 999px
        /// badge radius came out as an ellipse instead of a pill. Sets CornerRadius on a Border
        /// and RadiusX/RadiusY on a Rectangle, again whenever the element is resized.
        /// </summary>
        public static readonly DependencyProperty FitRadiusProperty =
            DependencyProperty.RegisterAttached("FitRadius", typeof(double), typeof(ThemeAssist),
                new PropertyMetadata(0.0, OnFitRadiusChanged));
        public static void SetFitRadius(DependencyObject target, double value) { target.SetValue(FitRadiusProperty, value); }
        public static double GetFitRadius(DependencyObject target) { return (double)target.GetValue(FitRadiusProperty); }

        private static void OnFitRadiusChanged(DependencyObject target, DependencyPropertyChangedEventArgs e)
        {
            var element = target as FrameworkElement;
            if (element == null) return;
            element.SizeChanged -= Refit;
            element.SizeChanged += Refit;
            Round(element);
        }

        private static void Refit(object sender, SizeChangedEventArgs e) { Round((FrameworkElement)sender); }

        private static void Round(FrameworkElement element)
        {
            var radius = Fit(GetFitRadius(element), element);
            var border = element as Border;
            if (border != null) { border.CornerRadius = new CornerRadius(radius); return; }
            var rectangle = element as System.Windows.Shapes.Rectangle;
            if (rectangle != null) { rectangle.RadiusX = radius; rectangle.RadiusY = radius; }
        }

        private static double Fit(double radius, FrameworkElement element)
        {
            if (radius <= 0) return 0;
            if (element.ActualWidth <= 0 || element.ActualHeight <= 0) return radius > 8 ? 0 : radius;
            return Math.Min(radius, Math.Min(element.ActualWidth, element.ActualHeight) / 2);
        }
    }

    /// <summary>
    /// Something drawn from a theme's own layers rather than a brush resource: repainted when
    /// the theme or Textures &amp; glow changes, and otherwise retained like any WPF drawing.
    /// Theme pins a theme (a settings tile); left unset it follows the visible one.
    /// </summary>
    public abstract class ThemeSurface : FrameworkElement
    {
        public static readonly DependencyProperty ThemeProperty =
            DependencyProperty.Register("Theme", typeof(ThemeMode?), typeof(ThemeSurface),
                new FrameworkPropertyMetadata(null, FrameworkPropertyMetadataOptions.AffectsRender));

        public ThemeMode? Theme
        {
            get { return (ThemeMode?)GetValue(ThemeProperty); }
            set { SetValue(ThemeProperty, value); }
        }

        protected ThemeSurface()
        {
            Loaded += (s, e) => { ThemeManager.Changed -= Repaint; ThemeManager.Changed += Repaint; InvalidateVisual(); };
            Unloaded += (s, e) => ThemeManager.Changed -= Repaint;
        }

        private void Repaint(object sender, EventArgs e) { InvalidateVisual(); }

        protected ThemeResources ThemeData
        {
            get { return ThemeResources.For(Theme ?? ThemeManager.Current, ThemeManager.EffectsEnabled); }
        }

        protected static void Paint(DrawingContext dc, System.Collections.Generic.IList<ThemeLayer> layers, Size size)
        {
            var area = new Rect(0, 0, size.Width, size.Height);
            // CSS lists the top layer first.
            for (int i = layers.Count - 1; i >= 0; i--) dc.DrawRectangle(layers[i].Create(size), null, area);
        }
    }

    /// <summary>The theme's background colour and, with effects on, its texture (stripes, grid, washes).</summary>
    public sealed class ThemeBackdrop : ThemeSurface
    {
        protected override void OnRender(DrawingContext dc)
        {
            var size = RenderSize;
            if (size.Width <= 0 || size.Height <= 0) return;
            var theme = ThemeData;
            dc.DrawRectangle(Css.Solid(theme.Colors["Panel"]), null, new Rect(size));
            if (theme.Effects) Paint(dc, theme.Theme.TextureLayers, size);
        }
    }

    /// <summary>
    /// The theme's overlay (scanlines, vignette) over everything in a window. It never takes
    /// input, and with effects off it draws nothing at all.
    /// </summary>
    public sealed class ThemeOverlay : ThemeSurface
    {
        public ThemeOverlay()
        {
            IsHitTestVisible = false;
            Focusable = false;
            SnapsToDevicePixels = true;
        }

        protected override void OnRender(DrawingContext dc)
        {
            var size = RenderSize;
            if (size.Width <= 0 || size.Height <= 0) return;
            var theme = ThemeData;
            if (theme.Effects) Paint(dc, theme.Theme.OverlayLayers, size);
        }
    }

    /// <summary>
    /// A status badge: the status colour's text and border, the theme's border width, style,
    /// radius, transform and tint, and a dot that pulses while the server runs. Foreground is
    /// the status colour, Background its tint.
    /// </summary>
    public sealed class StatusBadge : ContentControl
    {
        public static readonly DependencyProperty PulseProperty =
            DependencyProperty.Register("Pulse", typeof(bool), typeof(StatusBadge), new FrameworkPropertyMetadata(false));

        /// <summary>A running server: a round dot that breathes, and the theme's ok glow.</summary>
        public bool Pulse
        {
            get { return (bool)GetValue(PulseProperty); }
            set { SetValue(PulseProperty, value); }
        }
    }
}
