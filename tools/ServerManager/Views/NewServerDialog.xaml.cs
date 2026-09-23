using System.Collections.Generic;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace S2x.ServerManager.Views
{
    /// <summary>One line of the + New server dialog: a bundled starter, or the blank server.</summary>
    public sealed class StarterChoice
    {
        public string Title { get; set; }
        public string Summary { get; set; }
        public string Meta { get; set; }
        public bool Blank { get; set; }
        /// <summary>The bundled preset behind this line; the dialog only carries it.</summary>
        public object Source { get; set; }
    }

    public partial class NewServerDialog : Window
    {
        private StarterChoice _picked;

        private NewServerDialog()
        {
            InitializeComponent();
            barTitle.MouseLeftButtonDown += (s, e) => { if (e.ChangedButton == MouseButton.Left) DragMove(); };
            btnCancel.Click += (s, e) => { DialogResult = false; };
        }

        private void ChoicePicked(object sender, RoutedEventArgs e)
        {
            var button = sender as Button;
            if (button == null) return;
            _picked = button.DataContext as StarterChoice;
            DialogResult = true;
        }

        /// <summary>The line the host picked, or null when they backed out.</summary>
        public static StarterChoice Ask(IEnumerable<StarterChoice> choices)
        {
            var dialog = new NewServerDialog();
            dialog.lstChoices.ItemsSource = new List<StarterChoice>(choices);
            if (Application.Current != null && Application.Current.MainWindow != null &&
                Application.Current.MainWindow.IsLoaded && Application.Current.MainWindow != dialog)
                dialog.Owner = Application.Current.MainWindow;
            return dialog.ShowDialog() == true ? dialog._picked : null;
        }
    }
}
