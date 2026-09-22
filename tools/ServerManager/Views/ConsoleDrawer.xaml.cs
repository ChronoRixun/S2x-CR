using System.Windows;
using System.Windows.Controls;
using S2x.ServerManager.ViewModels;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// The console drawer. One of these sits over the fleet's cards and one under the editor;
    /// both show the same view model, so a server's console is the same console either way.
    /// </summary>
    public partial class ConsoleDrawer : UserControl
    {
        private ConsoleViewModel _console;

        public ConsoleDrawer()
        {
            InitializeComponent();
            DataContextChanged += Rebind;
            grip.DragDelta += (s, e) => { if (_console != null) _console.Height -= e.VerticalChange; };
            // Opened on a log that is already long, the newest line is the one wanted.
            Loaded += (s, e) => Appended();
            IsVisibleChanged += (s, e) => Appended();
        }

        private void Rebind(object sender, DependencyPropertyChangedEventArgs e)
        {
            var old = e.OldValue as ConsoleViewModel;
            if (old != null)
            {
                old.LinesAppended -= Appended;
                old.FilterFocusRequested -= FocusFilter;
            }
            _console = e.NewValue as ConsoleViewModel;
            if (_console == null) return;
            _console.LinesAppended += Appended;
            _console.FilterFocusRequested += FocusFilter;
        }

        /// <summary>FOLLOW: the newest line stays on screen. Off, the host keeps their place.</summary>
        private void Appended()
        {
            if (_console == null || !_console.IsFollowing || !IsVisible) return;
            Dispatcher.BeginInvoke((System.Action)(() => scroll.ScrollToEnd()),
                System.Windows.Threading.DispatcherPriority.Background);
        }

        /// <summary>Ctrl+L, answered by whichever drawer is the one on screen.</summary>
        private void FocusFilter()
        {
            if (!IsVisible) return;
            txtFilter.Focus();
            txtFilter.SelectAll();
        }
    }
}
