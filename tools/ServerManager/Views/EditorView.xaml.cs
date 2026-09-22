using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using S2x.ServerManager.ViewModels;

namespace S2x.ServerManager.Views
{
    public partial class EditorView : UserControl
    {
        private EditorViewModel _editor;

        public EditorView()
        {
            InitializeComponent();
            DataContextChanged += Rebind;
            // The colour swatches drop a code where the caret is, so the box has to say where
            // that is, and take the caret back afterwards.
            txtName.SelectionChanged += (s, e) => { if (_editor != null) _editor.CaretIndex = txtName.CaretIndex; };
            lstRotation.PreviewKeyDown += RotationKey;
        }

        private void Rebind(object sender, DependencyPropertyChangedEventArgs e)
        {
            var old = e.OldValue as EditorViewModel;
            if (old != null) old.CaretSet -= MoveCaret;
            _editor = e.NewValue as EditorViewModel;
            if (_editor != null) _editor.CaretSet += MoveCaret;
        }

        private void MoveCaret(int index)
        {
            txtName.Focus();
            txtName.CaretIndex = System.Math.Min(index, txtName.Text.Length);
        }

        private void RotationKey(object sender, KeyEventArgs e)
        {
            if (e.Key != Key.Delete || _editor == null || _editor.SelectedRow == null) return;
            _editor.RemoveSelected();
            e.Handled = true;
        }
    }
}
