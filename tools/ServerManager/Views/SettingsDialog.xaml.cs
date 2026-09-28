using System;
using System.Windows;
using System.Windows.Controls;
using S2x.ServerManager.Services;

namespace S2x.ServerManager.Views
{
    public partial class SettingsDialog : Window
    {
        private bool _changing;
        public SettingsDialog()
        {
            InitializeComponent();
            SelectCurrent();
            message.Text = ThemeManager.LoadWarning ?? "";
            theme.SelectionChanged += (sender, args) =>
            {
                if (_changing) return;
                var item = theme.SelectedItem as ComboBoxItem;
                if (item == null) return;
                try
                {
                    ThemeManager.Apply((ThemeMode)Enum.Parse(typeof(ThemeMode), (string)item.Tag));
                    message.Text = "";
                }
                catch (Exception ex)
                {
                    message.Text = "Could not save the theme. Your previous theme is unchanged. " + ex.Message;
                    SelectCurrent();
                }
            };
        }
        private void SelectCurrent()
        {
            _changing = true;
            foreach (ComboBoxItem item in theme.Items)
                if ((string)item.Tag == ThemeManager.Current.ToString()) theme.SelectedItem = item;
            _changing = false;
        }
        public static void Show(Window owner)
        {
            var dialog = new SettingsDialog();
            if (owner != null) dialog.Owner = owner;
            dialog.ShowDialog();
        }
    }
}
