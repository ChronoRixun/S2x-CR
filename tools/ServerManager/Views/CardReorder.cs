using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using S2x.ServerManager.ViewModels;

namespace S2x.ServerManager.Views
{
    /// <summary>Card-only drag payload; never starts over interactive child controls.</summary>
    public static class CardReorder
    {
        private sealed class Gesture { public Point Start; public bool Armed; }
        private sealed class Payload { public FleetViewModel Fleet; public ServerCardViewModel Card; }
        private static readonly HashSet<FrameworkElement> Hints = new HashSet<FrameworkElement>();
        private static readonly DependencyProperty GestureProperty = DependencyProperty.RegisterAttached("Gesture", typeof(Gesture), typeof(CardReorder));
        public static readonly DependencyProperty EnabledProperty = DependencyProperty.RegisterAttached("Enabled", typeof(bool), typeof(CardReorder), new PropertyMetadata(false, Changed));
        public static void SetEnabled(DependencyObject d, bool v) { d.SetValue(EnabledProperty, v); }
        public static bool GetEnabled(DependencyObject d) { return (bool)d.GetValue(EnabledProperty); }
        public static readonly DependencyProperty HintProperty = DependencyProperty.RegisterAttached("Hint", typeof(string), typeof(CardReorder), new PropertyMetadata(""));
        public static string GetHint(DependencyObject d) { return (string)d.GetValue(HintProperty); }
        public static void SetHint(DependencyObject d, string v) { d.SetValue(HintProperty, v); }
        private static void Changed(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var card = d as FrameworkElement;
            if (card == null) return;
            if ((bool)e.OldValue)
            {
                card.PreviewMouseLeftButtonDown -= Press; card.PreviewMouseMove -= Move;
                card.PreviewMouseLeftButtonUp -= Release; card.DragOver -= Over;
                card.DragLeave -= Leave; card.Drop -= Drop; card.PreviewKeyDown -= Key;
                card.Unloaded -= Unload;
            }
            card.AllowDrop = (bool)e.NewValue;
            if (!(bool)e.NewValue) { Clear(card); return; }
            card.SetValue(GestureProperty, new Gesture());
            card.PreviewMouseLeftButtonDown += Press; card.PreviewMouseMove += Move;
            card.PreviewMouseLeftButtonUp += Release; card.DragOver += Over;
            card.DragLeave += Leave; card.Drop += Drop; card.PreviewKeyDown += Key;
            card.Unloaded += Unload;
        }
        private static DependencyObject Parent(DependencyObject d)
        {
            var content = d as FrameworkContentElement;
            return content != null ? content.Parent : d is Visual ? VisualTreeHelper.GetParent(d) : LogicalTreeHelper.GetParent(d);
        }
        private static bool Interactive(DependencyObject source, DependencyObject card)
        {
            for (var d = source; d != null && d != card; d = Parent(d))
                if (d is ButtonBase || d is TextBoxBase || d is PasswordBox || d is Selector || d is Thumb || d is Hyperlink || d is MenuItem) return true;
            return false;
        }
        private static FleetViewModel Fleet(DependencyObject card)
        {
            for (var d = card; d != null; d = Parent(d))
            {
                var element = d as FrameworkElement;
                var fleet = element == null ? null : element.DataContext as FleetViewModel;
                if (fleet != null) return fleet;
            }
            return null;
        }
        private static void Press(object sender, MouseButtonEventArgs e)
        {
            var card = (FrameworkElement)sender; var gesture = (Gesture)card.GetValue(GestureProperty);
            gesture.Armed = !Interactive(e.OriginalSource as DependencyObject, card);
            if (gesture.Armed) { gesture.Start = e.GetPosition(card); card.Focus(); }
        }
        private static void Release(object sender, MouseButtonEventArgs e) { ((Gesture)((FrameworkElement)sender).GetValue(GestureProperty)).Armed = false; }
        private static void Move(object sender, MouseEventArgs e)
        {
            var element = (FrameworkElement)sender; var gesture = (Gesture)element.GetValue(GestureProperty);
            if (e.LeftButton != MouseButtonState.Pressed) { gesture.Armed = false; return; }
            if (!gesture.Armed) return;
            var point = e.GetPosition(element);
            if (Math.Abs(point.X - gesture.Start.X) < SystemParameters.MinimumHorizontalDragDistance && Math.Abs(point.Y - gesture.Start.Y) < SystemParameters.MinimumVerticalDragDistance) return;
            gesture.Armed = false;
            var fleet = Fleet(element); var card = element.DataContext as ServerCardViewModel;
            if (fleet == null || card == null) return;
            try { DragDrop.DoDragDrop(element, new DataObject(typeof(Payload), new Payload { Fleet = fleet, Card = card }), DragDropEffects.Move); }
            finally { foreach (var hinted in new List<FrameworkElement>(Hints)) Clear(hinted); }
        }
        private static Payload Read(DragEventArgs e) { return e.Data.GetDataPresent(typeof(Payload)) ? e.Data.GetData(typeof(Payload)) as Payload : null; }
        private static bool Valid(FrameworkElement element, Payload payload)
        { return payload != null && payload.Fleet == Fleet(element) && element.DataContext is ServerCardViewModel && payload.Card != element.DataContext; }
        private static void Over(object sender, DragEventArgs e)
        {
            var element = (FrameworkElement)sender; var payload = Read(e);
            Clear(element); e.Effects = DragDropEffects.None;
            if (Valid(element, payload)) { SetHint(element, e.GetPosition(element).X < element.ActualWidth / 2 ? "Place before" : "Place after"); Hints.Add(element); e.Effects = DragDropEffects.Move; }
            e.Handled = true;
        }
        private static void Drop(object sender, DragEventArgs e)
        {
            var element = (FrameworkElement)sender; var payload = Read(e); Clear(element);
            if (Valid(element, payload)) payload.Fleet.MoveCard(payload.Card, (ServerCardViewModel)element.DataContext, e.GetPosition(element).X >= element.ActualWidth / 2);
            e.Handled = true;
        }
        private static void Key(object sender, KeyEventArgs e)
        {
            var element = (FrameworkElement)sender;
            if (Interactive(e.OriginalSource as DependencyObject, element) || Keyboard.Modifiers != ModifierKeys.Alt) return;
            var key = e.Key == System.Windows.Input.Key.System ? e.SystemKey : e.Key;
            if (key != System.Windows.Input.Key.Left && key != System.Windows.Input.Key.Right) return;
            var fleet = Fleet(element); var card = element.DataContext as ServerCardViewModel;
            if (fleet == null || card == null) return;
            fleet.MoveCardBy(card, key == System.Windows.Input.Key.Left ? -1 : 1); e.Handled = true;
            // Rebuilding Shown recreates the template. Restore focus to the moved card.
            var root = Window.GetWindow(element);
            if (root != null) root.Dispatcher.BeginInvoke(new Action(() => FocusCard(root, card)));
        }
        private static bool FocusCard(DependencyObject d, ServerCardViewModel card)
        {
            var element = d as FrameworkElement;
            if (element != null && GetEnabled(element) && element.DataContext == card) return element.Focus();
            for (int i = 0; i < VisualTreeHelper.GetChildrenCount(d); i++) if (FocusCard(VisualTreeHelper.GetChild(d, i), card)) return true;
            return false;
        }
        private static void Leave(object sender, DragEventArgs e) { Clear((FrameworkElement)sender); }
        private static void Unload(object sender, RoutedEventArgs e) { Clear((FrameworkElement)sender); }
        private static void Clear(FrameworkElement card) { SetHint(card, ""); Hints.Remove(card); }
    }
}
