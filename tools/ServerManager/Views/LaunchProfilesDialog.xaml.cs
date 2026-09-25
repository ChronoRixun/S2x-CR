using System;
using System.Linq;
using System.Windows;
using System.Windows.Input;
using S2x.ServerManager.Models;
using S2x.ServerManager.Services;

namespace S2x.ServerManager.Views
{
    /// <summary>One registered folder, as the dialog shows it.</summary>
    internal sealed class ProfileRow
    {
        public string Folder { get; set; }
        public string Summary { get; set; }
        public string Problem { get; set; }
        public Visibility ProblemVisibility { get { return string.IsNullOrEmpty(Problem) ? Visibility.Collapsed : Visibility.Visible; } }
    }

    /// <summary>
    /// The folders in &lt;game&gt;\s2x\launch-profiles.txt: add one, remove one. Removing only
    /// unregisters it, so it is not asked about; a host who prefers files edits the list by hand.
    /// </summary>
    public partial class LaunchProfilesDialog : Window
    {
        private readonly string _gameDir;

        private LaunchProfilesDialog(string gameDir)
        {
            InitializeComponent();
            _gameDir = gameDir;
            barTitle.MouseLeftButtonDown += (s, e) => { if (e.ChangedButton == MouseButton.Left) DragMove(); };
            btnClose.Click += (s, e) => Close();
            btnAdd.Click += (s, e) => Add();
            lblFile.Text = LaunchProfiles.ListPath(gameDir);
            lblFile.ToolTip = lblFile.Text;
            Reload();
        }

        private void Reload()
        {
            var rows = LaunchProfiles.Load(_gameDir).Select(profile => new ProfileRow
            {
                Folder = profile.Folder,
                Summary = profile.Id == null
                    ? "not found"
                    : profile.Title + "  " + GameData.MiddleDot + "  " + profile.Entries.Count +
                      (profile.Entries.Count == 1 ? " mode" : " modes"),
                Problem = profile.Problem,
            }).ToList();
            lstProfiles.ItemsSource = rows;
            lblEmpty.Visibility = rows.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        }

        private void Say(string message)
        {
            lblMessage.Text = message ?? "";
            lblMessage.Visibility = string.IsNullOrEmpty(message) ? Visibility.Collapsed : Visibility.Visible;
        }

        private void Add()
        {
            string folder;
            using (var picker = new System.Windows.Forms.FolderBrowserDialog())
            {
                picker.Description = "Select the folder that holds " + LaunchProfiles.ProfileFile;
                picker.ShowNewFolderButton = false;
                if (picker.ShowDialog() != System.Windows.Forms.DialogResult.OK) return;
                folder = picker.SelectedPath;
            }

            // Only a file that parses makes a folder a profile; anything else would only ever
            // say not found.
            var profile = LaunchProfiles.Read(folder);
            if (profile.Id == null) { Say("Not added: " + profile.Problem + "."); return; }
            Change(() => LaunchProfiles.Add(_gameDir, folder));
        }

        private void RemovePicked(object sender, RoutedEventArgs e)
        {
            var row = ((FrameworkElement)sender).DataContext as ProfileRow;
            if (row != null) Change(() => LaunchProfiles.Remove(_gameDir, row.Folder));
        }

        private void Change(Action write)
        {
            try
            {
                write();
                Say(null);
            }
            catch (Exception ex)
            {
                Say("Could not write the list: " + ex.Message);
            }
            Reload();
        }

        /// <summary>Shows the list until the host closes it. The caller reads the profiles again afterwards.</summary>
        internal static void Manage(string gameDir)
        {
            var dialog = new LaunchProfilesDialog(gameDir);
            if (Application.Current != null && Application.Current.MainWindow != null &&
                Application.Current.MainWindow.IsLoaded && Application.Current.MainWindow != dialog)
                dialog.Owner = Application.Current.MainWindow;
            dialog.ShowDialog();
        }
    }
}
