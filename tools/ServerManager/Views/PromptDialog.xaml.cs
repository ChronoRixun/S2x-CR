using System.Windows;
using System.Windows.Input;

namespace S2x.ServerManager.Views
{
    /// <summary>The launcher's Show-Prompt: a title, a label, one box, Save or Cancel.</summary>
    public partial class PromptDialog : Window
    {
        private PromptDialog()
        {
            InitializeComponent();
            barTitle.MouseLeftButtonDown += (s, e) => { if (e.ChangedButton == MouseButton.Left) DragMove(); };
            btnOk.Click += (s, e) => { DialogResult = true; };
            btnCancel.Click += (s, e) => { DialogResult = false; };
            Loaded += (s, e) => { txtValue.Focus(); txtValue.SelectAll(); };
        }

        /// <summary>The text the host typed, or null when they backed out.</summary>
        public static string Ask(string title, string label, string initial)
        {
            var dialog = new PromptDialog();
            TextEffects.SetTracked(dialog.lblTitle, (title ?? "").ToUpperInvariant());
            dialog.lblLabel.Text = label;
            dialog.txtValue.Text = initial ?? "";
            if (Application.Current != null && Application.Current.MainWindow != null &&
                Application.Current.MainWindow.IsLoaded && Application.Current.MainWindow != dialog)
                dialog.Owner = Application.Current.MainWindow;
            return dialog.ShowDialog() == true ? dialog.txtValue.Text : null;
        }
    }
}
