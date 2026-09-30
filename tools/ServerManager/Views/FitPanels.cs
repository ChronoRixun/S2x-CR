using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// A row laid out the way the design's CSS grid rows are (1.2fr 0.9fr 1.1fr ...): each child
    /// is at least as wide as its content, and the width left over is shared out by Weight. WPF
    /// star columns have no such minimum, so a wide theme font (Press Start 2P) cut STOP to "STO".
    /// A child with Shrink set may go narrower than its content, down to its MinWidth, when the
    /// row is short of room; it should trim its own text. Collapsed children take no room.
    /// A row with no weighted child leaves what it does not need empty on the right.
    /// </summary>
    public sealed class FitRow : Panel
    {
        public static readonly DependencyProperty WeightProperty =
            DependencyProperty.RegisterAttached("Weight", typeof(double), typeof(FitRow),
                new FrameworkPropertyMetadata(0.0, FrameworkPropertyMetadataOptions.AffectsParentMeasure));
        public static void SetWeight(UIElement target, double value) { target.SetValue(WeightProperty, value); }
        public static double GetWeight(UIElement target) { return (double)target.GetValue(WeightProperty); }

        public static readonly DependencyProperty ShrinkProperty =
            DependencyProperty.RegisterAttached("Shrink", typeof(bool), typeof(FitRow),
                new FrameworkPropertyMetadata(false, FrameworkPropertyMetadataOptions.AffectsParentMeasure));
        public static void SetShrink(UIElement target, bool value) { target.SetValue(ShrinkProperty, value); }
        public static bool GetShrink(UIElement target) { return (bool)target.GetValue(ShrinkProperty); }

        private readonly List<double> _natural = new List<double>();

        /// <summary>
        /// The narrowest this row can be without cutting anything: every child at its content
        /// width, shrinkable ones at their MinWidth. Known after a measure; SplitRow reads it.
        /// </summary>
        public double MinimumWidth { get; private set; }

        protected override Size MeasureOverride(Size available)
        {
            _natural.Clear();
            double height = 0, minimum = 0;
            foreach (UIElement child in InternalChildren)
            {
                child.Measure(new Size(double.PositiveInfinity, available.Height));
                var natural = child.Visibility == Visibility.Collapsed ? 0 : child.DesiredSize.Width;
                _natural.Add(natural);
                minimum += GetShrink(child) ? Math.Min(natural, Floor(child)) : natural;
            }
            MinimumWidth = minimum;
            var widths = Widths(available.Width);
            double total = 0;
            for (int i = 0; i < InternalChildren.Count; i++)
            {
                var child = InternalChildren[i];
                if (child.Visibility == Visibility.Collapsed) continue;
                // Only a child that gets less than its content is measured again, at the width it
                // will get, so a trimming TextBlock trims. The others keep their unconstrained
                // measure: WPF re-measures a child with its last constraint, and a finite one
                // would hide text that arrives later (the breadcrumb stayed at width 0).
                if (widths[i] < _natural[i] - 0.5) child.Measure(new Size(Math.Max(0, widths[i]), available.Height));
                height = Math.Max(height, child.DesiredSize.Height);
                total += widths[i];
            }
            return new Size(double.IsInfinity(available.Width) ? total : Math.Min(total, available.Width), height);
        }

        protected override Size ArrangeOverride(Size final)
        {
            var widths = Widths(final.Width);
            double x = 0;
            for (int i = 0; i < InternalChildren.Count; i++)
            {
                var child = InternalChildren[i];
                if (child.Visibility == Visibility.Collapsed) { child.Arrange(new Rect(x, 0, 0, final.Height)); continue; }
                child.Arrange(new Rect(x, 0, widths[i], final.Height));
                x += widths[i];
            }
            return final;
        }

        /// <summary>A shrinkable child's least width: its MinWidth and its own margins.</summary>
        private static double Floor(UIElement child)
        {
            var element = child as FrameworkElement;
            return element == null ? 0 : element.MinWidth + element.Margin.Left + element.Margin.Right;
        }

        private double[] Widths(double available)
        {
            var count = InternalChildren.Count;
            var widths = new double[count];
            var minimum = new double[count];
            var weight = new double[count];
            double fixedTotal = 0, minimumTotal = 0, weightTotal = 0;
            for (int i = 0; i < count; i++)
            {
                var child = InternalChildren[i];
                var natural = i < _natural.Count ? _natural[i] : 0;
                if (child.Visibility == Visibility.Collapsed) continue;
                minimum[i] = GetShrink(child) ? Math.Min(natural, Floor(child)) : natural;
                weight[i] = Math.Max(0, GetWeight(child));
                widths[i] = natural;
                minimumTotal += minimum[i];
                if (weight[i] > 0) weightTotal += weight[i];
                else fixedTotal += natural;
            }
            if (double.IsInfinity(available) || double.IsNaN(available)) return widths;

            if (weightTotal > 0)
            {
                // Weighted children: max(minimum, weight x unit), with the unit chosen so the row
                // is exactly as wide as it was given, found by bisection.
                double weightedMinimum = 0;
                for (int i = 0; i < count; i++) if (weight[i] > 0) weightedMinimum += minimum[i];
                var room = available - fixedTotal;
                if (room >= weightedMinimum)
                {
                    double low = 0, high = Math.Max(room, 1) / weightTotal * 4 + 1;
                    for (int step = 0; step < 40; step++)
                    {
                        var unit = (low + high) / 2;
                        double used = 0;
                        for (int i = 0; i < count; i++) if (weight[i] > 0) used += Math.Max(minimum[i], weight[i] * unit);
                        if (used > room) high = unit; else low = unit;
                    }
                    double sum = 0;
                    int last = -1;
                    for (int i = 0; i < count; i++)
                    {
                        if (weight[i] <= 0 || InternalChildren[i].Visibility == Visibility.Collapsed) continue;
                        widths[i] = Math.Max(minimum[i], weight[i] * low);
                        sum += widths[i];
                        last = i;
                    }
                    if (last >= 0) widths[last] += room - sum;   // rounding: the row is exactly full
                    return widths;
                }
                for (int i = 0; i < count; i++) if (weight[i] > 0) widths[i] = minimum[i];
            }

            // Short of room: shrinkable children give up what they have above their minimum,
            // the right-most first, so a row reads whole from the left for as long as it can.
            double total = 0;
            for (int i = 0; i < count; i++) total += widths[i];
            var deficit = total - available;
            for (int i = count - 1; i >= 0 && deficit > 0; i--)
            {
                var give = Math.Min(deficit, widths[i] - minimum[i]);
                if (give <= 0) continue;
                widths[i] -= give;
                deficit -= give;
            }
            return widths;
        }
    }

    /// <summary>
    /// A label and its control on one line, the control against the right edge, when both fit;
    /// otherwise the label on a line of its own and the control under it. The editor's Name pool
    /// and Difficulty rows use it: in Orbitron or Press Start 2P the four difficulty buttons
    /// alone nearly fill the column, and the label was being cut under them. The fleet's stat
    /// strip uses it too, as the design's strip wraps: a label that is a FitRow counts at its
    /// minimum width, so the strip only stacks when the stats cannot shrink enough.
    /// </summary>
    public sealed class SplitRow : Panel
    {
        public static readonly DependencyProperty GapProperty =
            DependencyProperty.Register("Gap", typeof(double), typeof(SplitRow),
                new FrameworkPropertyMetadata(12.0, FrameworkPropertyMetadataOptions.AffectsMeasure));

        public double Gap
        {
            get { return (double)GetValue(GapProperty); }
            set { SetValue(GapProperty, value); }
        }

        /// <summary>
        /// The least width the label needs beside the control, when it stretches and trims and
        /// so has no width of its own to go by (the editor's name box and swatch row). 0 uses
        /// the label's content width, or a FitRow's minimum.
        /// </summary>
        public static readonly DependencyProperty LabelMinWidthProperty =
            DependencyProperty.Register("LabelMinWidth", typeof(double), typeof(SplitRow),
                new FrameworkPropertyMetadata(0.0, FrameworkPropertyMetadataOptions.AffectsMeasure));

        public double LabelMinWidth
        {
            get { return (double)GetValue(LabelMinWidthProperty); }
            set { SetValue(LabelMinWidthProperty, value); }
        }

        private bool _stacked;

        protected override Size MeasureOverride(Size available)
        {
            if (InternalChildren.Count < 2) return new Size(0, 0);
            var label = InternalChildren[0];
            var control = InternalChildren[1];
            label.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
            control.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
            var row = label as FitRow;
            var labelNeeds = LabelMinWidth > 0 ? LabelMinWidth : row != null ? row.MinimumWidth : label.DesiredSize.Width;
            var one = labelNeeds + Gap + control.DesiredSize.Width;
            _stacked = !double.IsInfinity(available.Width) && one > available.Width;
            if (!_stacked)
            {
                if (!double.IsInfinity(available.Width))
                    label.Measure(new Size(Math.Max(0, available.Width - Gap - control.DesiredSize.Width), double.PositiveInfinity));
                return new Size(double.IsInfinity(available.Width) ? label.DesiredSize.Width + Gap + control.DesiredSize.Width : available.Width,
                                Math.Max(label.DesiredSize.Height, control.DesiredSize.Height));
            }
            label.Measure(new Size(available.Width, double.PositiveInfinity));
            control.Measure(new Size(available.Width, double.PositiveInfinity));
            return new Size(available.Width, label.DesiredSize.Height + Gap / 2 + control.DesiredSize.Height);
        }

        protected override Size ArrangeOverride(Size final)
        {
            if (InternalChildren.Count < 2) return final;
            var label = InternalChildren[0];
            var control = InternalChildren[1];
            if (_stacked)
            {
                label.Arrange(new Rect(0, 0, final.Width, label.DesiredSize.Height));
                var top = label.DesiredSize.Height + Gap / 2;
                control.Arrange(new Rect(0, top, Math.Min(final.Width, control.DesiredSize.Width), control.DesiredSize.Height));
                return final;
            }
            var width = control.DesiredSize.Width;
            control.Arrange(new Rect(Math.Max(0, final.Width - width), 0, Math.Min(width, final.Width), final.Height));
            // The label has everything left of the control, so one that stretches fills it.
            label.Arrange(new Rect(0, 0, Math.Max(0, final.Width - width - Gap), final.Height));
            return final;
        }
    }

    /// <summary>
    /// Visible while a width is at least the parameter, collapsed below it: a column that steps
    /// aside in a narrow list (the rotation row's points, in the roster at the minimum window
    /// size) so the map name keeps room to show.
    /// </summary>
    public sealed class WidthAtLeast : System.Windows.Data.IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            var width = value is double ? (double)value : 0;
            var needed = System.Convert.ToDouble(parameter, System.Globalization.CultureInfo.InvariantCulture);
            // Unmeasured (0) counts as wide: a first layout pass must not hide anything.
            return width <= 0 || width >= needed ? Visibility.Visible : Visibility.Collapsed;
        }

        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            throw new NotSupportedException();
        }
    }

    /// <summary>
    /// The height of a text box that shows the parameter's number of lines in its own font, from
    /// its FontSize, FontFamily, Padding and BorderThickness (in that order), so a theme with a
    /// taller font gets a taller box. TextBox.MinLines/MaxLines would do this, but a TextBox with
    /// them inside a collapsed view (the fleet window holds a hidden editor) never finishes its
    /// layout and keeps the dispatcher busy for good.
    /// </summary>
    public sealed class LinesHeight : System.Windows.Data.IMultiValueConverter
    {
        public object Convert(object[] values, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            var size = values.Length > 0 && values[0] is double ? (double)values[0] : 12;
            var family = values.Length > 1 ? values[1] as System.Windows.Media.FontFamily : null;
            var padding = values.Length > 2 && values[2] is Thickness ? (Thickness)values[2] : new Thickness();
            var border = values.Length > 3 && values[3] is Thickness ? (Thickness)values[3] : new Thickness();
            var lines = System.Convert.ToInt32(parameter, System.Globalization.CultureInfo.InvariantCulture);
            var spacing = family != null && family.LineSpacing > 0 ? family.LineSpacing : 1.3;
            return Math.Ceiling(lines * spacing * size + padding.Top + padding.Bottom + border.Top + border.Bottom);
        }

        public object[] ConvertBack(object value, Type[] targetTypes, object parameter, System.Globalization.CultureInfo culture)
        {
            throw new NotSupportedException();
        }
    }
}
