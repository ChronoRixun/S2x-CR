using System.Reflection;
using System.Windows;
using System.Windows.Input;

namespace S2x.ServerManager
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            barTitle.MouseLeftButtonDown += TitleBarPressed;
            btnMinimize.Click += (s, e) => WindowState = WindowState.Minimized;
            btnMaximize.Click += (s, e) => ToggleMaximized();
            btnClose.Click += (s, e) => Close();
            txtVersion.Text = "v" + Assembly.GetExecutingAssembly().GetName().Version.ToString(3);
        }

        private void OpenSettings(object sender, RoutedEventArgs e)
        {
            Views.SettingsDialog.Show(this);
        }

        private void OpenMasterList(object sender, RoutedEventArgs e)
        {
            new Views.MasterBrowserWindow { Owner = this }.ShowDialog();
        }

        private void TitleBarPressed(object sender, MouseButtonEventArgs e)
        {
            if (e.ClickCount == 2) ToggleMaximized();
            else DragMove();
        }

        private void ToggleMaximized()
        {
            WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;
        }
    }
}
