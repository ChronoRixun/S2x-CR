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
        private bool _loaded;

        public ConsoleDrawer()
        {
            InitializeComponent();
            DataContextChanged += (s, e) => Listen();
            grip.DragDelta += (s, e) => { if (_console != null) _console.Height -= e.VerticalChange; };
            // Opened on a log that is already long, the newest line is the one wanted.
            Loaded += (s, e) => { _loaded = true; Listen(); Appended(); };
            Unloaded += (s, e) => { _loaded = false; Listen(); };
            IsVisibleChanged += (s, e) => Appended();
        }

        private void OpenAdmin(object sender, RoutedEventArgs e)
        {
            if (_console?.Card == null) return;
            new AdminWindow(_console.Card.Preset.Port, _console.Card.Name) { Owner = Window.GetWindow(this) }.ShowDialog();
        }

        /// <summary>
        /// Listens to the console only while this drawer is loaded: the console lives as long as
        /// the fleet, and a handler left on it kept a closed window alive with it.
        /// </summary>
        private void Listen()
        {
            var wanted = _loaded ? DataContext as ConsoleViewModel : null;
            if (wanted == _console) return;
            if (_console != null)
            {
                _console.LinesAppended -= Appended;
                _console.FilterFocusRequested -= FocusFilter;
            }
            _console = wanted;
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
