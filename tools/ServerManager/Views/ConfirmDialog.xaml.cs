using System.Windows;
using System.Windows.Input;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// PromptDialog without the box: a title, what is about to happen, and the two answers.
    /// Cancel is the default, so leaning on Enter never destroys anything.
    /// </summary>
    public partial class ConfirmDialog : Window
    {
        private ConfirmDialog()
        {
            InitializeComponent();
            barTitle.MouseLeftButtonDown += (s, e) => { if (e.ChangedButton == MouseButton.Left) DragMove(); };
            btnOk.Click += (s, e) => { DialogResult = true; };
            btnCancel.Click += (s, e) => { DialogResult = false; };
            Loaded += (s, e) => btnCancel.Focus();
        }

        /// <summary>True only when the host picked the named action.</summary>
        public static bool Ask(string title, string question, string detail, string action)
        {
            var dialog = new ConfirmDialog();
            TextEffects.SetTracked(dialog.lblTitle, (title ?? "").ToUpperInvariant());
            dialog.lblQuestion.Text = question;
            dialog.lblDetail.Text = detail;
            dialog.btnOk.Content = action;
            if (Application.Current != null && Application.Current.MainWindow != null &&
                Application.Current.MainWindow.IsLoaded && Application.Current.MainWindow != dialog)
                dialog.Owner = Application.Current.MainWindow;
            return dialog.ShowDialog() == true;
        }
    }
}
