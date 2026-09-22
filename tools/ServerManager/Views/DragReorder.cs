using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using S2x.ServerManager.ViewModels;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// Mouse drag to reorder the rotation. The row carries itself as the payload, the row under
    /// the pointer lights up, and the drop moves one onto the other. A press on a button inside
    /// the row is that button's, not a drag.
    /// </summary>
    public static class DragReorder
    {
        private const double Threshold = 5;
        private static Point _start;
        private static bool _armed;

        public static readonly DependencyProperty EnabledProperty =
            DependencyProperty.RegisterAttached("Enabled", typeof(bool), typeof(DragReorder),
                new PropertyMetadata(false, OnEnabledChanged));

        public static void SetEnabled(DependencyObject target, bool value) { target.SetValue(EnabledProperty, value); }
        public static bool GetEnabled(DependencyObject target) { return (bool)target.GetValue(EnabledProperty); }

        private static void OnEnabledChanged(DependencyObject target, DependencyPropertyChangedEventArgs e)
        {
            var row = target as FrameworkElement;
            if (row == null || !(e.NewValue is bool) || !(bool)e.NewValue) return;

            row.AllowDrop = true;
            row.PreviewMouseLeftButtonDown += Pressed;
            row.PreviewMouseMove += Moved;
            row.PreviewMouseLeftButtonUp += Released;
            row.DragOver += Over;
            row.DragLeave += Left;
            row.Drop += Dropped;
        }

        private static void Pressed(object sender, MouseButtonEventArgs e)
        {
            // Let the row's own buttons have their click.
            if (e.OriginalSource is DependencyObject && IsInsideButton((DependencyObject)e.OriginalSource)) return;
            _start = e.GetPosition(null);
            _armed = true;
        }

        private static void Released(object sender, MouseButtonEventArgs e) { _armed = false; }

        private static void Moved(object sender, MouseEventArgs e)
        {
            if (!_armed || e.LeftButton != MouseButtonState.Pressed) return;
            var now = e.GetPosition(null);
            if (Math.Abs(now.X - _start.X) < Threshold && Math.Abs(now.Y - _start.Y) < Threshold) return;

            var row = Row(sender);
            if (row == null) { _armed = false; return; }

            _armed = false;
            row.IsDragging = true;
            try { DragDrop.DoDragDrop((DependencyObject)sender, row, DragDropEffects.Move); }
            finally { row.IsDragging = false; }
        }

        private static void Over(object sender, DragEventArgs e)
        {
            var row = Row(sender);
            var dragged = Payload(e);
            e.Effects = dragged == null || dragged == row ? DragDropEffects.None : DragDropEffects.Move;
            if (row != null && dragged != null && dragged != row) row.IsDropTarget = true;
            e.Handled = true;
        }

        private static void Left(object sender, DragEventArgs e)
        {
            var row = Row(sender);
            if (row != null) row.IsDropTarget = false;
        }

        private static void Dropped(object sender, DragEventArgs e)
        {
            var row = Row(sender);
            var dragged = Payload(e);
            if (row != null) row.IsDropTarget = false;
            if (row == null || dragged == null || dragged == row) return;
            row.Editor.MoveRow(dragged, row);
            e.Handled = true;
        }

        private static RotationRowViewModel Row(object sender)
        {
            var element = sender as FrameworkElement;
            return element == null ? null : element.DataContext as RotationRowViewModel;
        }

        private static RotationRowViewModel Payload(DragEventArgs e)
        {
            var type = typeof(RotationRowViewModel);
            return e.Data.GetDataPresent(type) ? e.Data.GetData(type) as RotationRowViewModel : null;
        }

        private static bool IsInsideButton(DependencyObject source)
        {
            for (var node = source; node != null; node = VisualOrLogicalParent(node))
                if (node is ButtonBase) return true;
            return false;
        }

        private static DependencyObject VisualOrLogicalParent(DependencyObject node)
        {
            if (node is System.Windows.Media.Visual) return System.Windows.Media.VisualTreeHelper.GetParent(node);
            return LogicalTreeHelper.GetParent(node);
        }
    }
}
