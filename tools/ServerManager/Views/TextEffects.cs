using System;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Media;
using S2x.ServerManager.Models;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// Two things WPF text does not do on its own: the engine's ^0-^7 colour runs, and the
    /// letter spacing the mockup uses on its small caps labels.
    /// </summary>
    public static class TextEffects
    {
        // ── colour codes ──────────────────────────────────────────────────────────
        public static readonly DependencyProperty ColorNameProperty =
            DependencyProperty.RegisterAttached("ColorName", typeof(string), typeof(TextEffects),
                new PropertyMetadata(null, OnColorNameChanged));

        public static void SetColorName(DependencyObject target, string value) { target.SetValue(ColorNameProperty, value); }
        public static string GetColorName(DependencyObject target) { return (string)target.GetValue(ColorNameProperty); }

        private static void OnColorNameChanged(DependencyObject target, DependencyPropertyChangedEventArgs e)
        {
            var block = target as TextBlock;
            if (block == null) return;
            block.Inlines.Clear();

            var name = e.NewValue as string;
            if (string.IsNullOrEmpty(name))
            {
                block.Inlines.Add(new Run("Unnamed server") { Foreground = Palette.Dim });
                return;
            }

            var color = GameData.ColorCodes['7'];
            var run = new StringBuilder();
            for (int i = 0; i < name.Length; i++)
            {
                if (name[i] == '^' && i + 1 < name.Length && name[i + 1] >= '0' && name[i + 1] <= '9')
                {
                    Flush(block, run, color);
                    string next;
                    if (GameData.ColorCodes.TryGetValue(name[i + 1], out next)) color = next;
                    i++;
                    continue;
                }
                run.Append(name[i]);
            }
            Flush(block, run, color);
            if (block.Inlines.Count == 0) block.Inlines.Add(new Run("Unnamed server") { Foreground = Palette.Dim });
        }

        private static void Flush(TextBlock block, StringBuilder run, string color)
        {
            if (run.Length == 0) return;
            block.Inlines.Add(new Run(run.ToString()) { Foreground = Palette.Frozen(color) });
            run.Clear();
        }

        // ── letter spacing ────────────────────────────────────────────────────────
        public static readonly DependencyProperty TrackedProperty =
            DependencyProperty.RegisterAttached("Tracked", typeof(string), typeof(TextEffects),
                new PropertyMetadata(null, OnTrackedChanged));

        public static void SetTracked(DependencyObject target, string value) { target.SetValue(TrackedProperty, value); }
        public static string GetTracked(DependencyObject target) { return (string)target.GetValue(TrackedProperty); }

        private static void OnTrackedChanged(DependencyObject target, DependencyPropertyChangedEventArgs e)
        {
            var block = target as TextBlock;
            if (block == null) return;
            var text = e.NewValue as string ?? "";
            var spaced = new StringBuilder(text.Length * 2);
            for (int i = 0; i < text.Length; i++)
            {
                spaced.Append(text[i]);
                if (i + 1 < text.Length) spaced.Append(' ');   // thin space
            }
            block.Text = spaced.ToString();
        }
    }

    /// <summary>
    /// Cards laid out like the mockup's grid: equal columns, every row as tall as its own
    /// tallest card. UniformGrid would make every row as tall as the tallest card anywhere.
    /// </summary>
    public sealed class CardsPanel : System.Windows.Controls.Panel
    {
        public static readonly DependencyProperty ColumnsProperty =
            DependencyProperty.Register("Columns", typeof(int), typeof(CardsPanel),
                new FrameworkPropertyMetadata(3, FrameworkPropertyMetadataOptions.AffectsMeasure));

        public int Columns
        {
            get { return (int)GetValue(ColumnsProperty); }
            set { SetValue(ColumnsProperty, value); }
        }

        protected override Size MeasureOverride(Size available)
        {
            var columns = Math.Max(1, Columns);
            var width = double.IsInfinity(available.Width) ? 1116 : available.Width;
            var cell = width / columns;

            double total = 0, row = 0;
            for (int i = 0; i < InternalChildren.Count; i++)
            {
                var child = InternalChildren[i];
                child.Measure(new Size(cell, double.PositiveInfinity));
                row = Math.Max(row, child.DesiredSize.Height);
                if ((i + 1) % columns == 0) { total += row; row = 0; }
            }
            return new Size(width, total + row);
        }

        protected override Size ArrangeOverride(Size final)
        {
            var columns = Math.Max(1, Columns);
            var cell = final.Width / columns;

            double y = 0, row = 0;
            int start = 0;
            for (int i = 0; i < InternalChildren.Count; i++)
            {
                row = Math.Max(row, InternalChildren[i].DesiredSize.Height);
                if ((i + 1) % columns != 0 && i != InternalChildren.Count - 1) continue;

                for (int j = start; j <= i; j++)
                    InternalChildren[j].Arrange(new Rect((j - start) * cell, y, cell, row));

                y += row;
                row = 0;
                start = i + 1;
            }
            return new Size(final.Width, Math.Max(y, 0));
        }
    }
}
