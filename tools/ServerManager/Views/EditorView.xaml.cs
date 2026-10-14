using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using S2x.ServerManager.ViewModels;

namespace S2x.ServerManager.Views
{
    public partial class EditorView : UserControl
    {
        /// <summary>
        /// The mockup's compact editor, for the roster's pane: the same screen in a narrower
        /// column, with the labels that do not fit dropped.
        /// </summary>
        public static readonly DependencyProperty CompactProperty =
            DependencyProperty.Register("Compact", typeof(bool), typeof(EditorView), new PropertyMetadata(false));

        public bool Compact
        {
            get { return (bool)GetValue(CompactProperty); }
            set { SetValue(CompactProperty, value); }
        }

        private EditorViewModel _editor;
        private bool _loaded;

        public EditorView()
        {
            InitializeComponent();
            DataContextChanged += (s, e) => Listen();
            Loaded += (s, e) => { _loaded = true; Listen(); };
            Unloaded += (s, e) => { _loaded = false; Listen(); };
            // The colour swatches drop a code where the caret is, so the box has to say where
            // that is, and take the caret back afterwards.
            txtName.SelectionChanged += (s, e) => { if (_editor != null) _editor.CaretIndex = txtName.CaretIndex; };
            lstRotation.PreviewKeyDown += RotationKey;
        }

        /// <summary>
        /// Listens to the editor only while this view is loaded. The fleet keeps an editor per
        /// preset for as long as it runs, and a handler left on one kept a closed window, with
        /// everything drawn in it, alive as long as the editor was.
        /// </summary>
        private void Listen()
        {
            var wanted = _loaded ? DataContext as EditorViewModel : null;
            if (wanted == _editor) return;
            if (_editor != null) _editor.CaretSet -= MoveCaret;
            _editor = wanted;
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
