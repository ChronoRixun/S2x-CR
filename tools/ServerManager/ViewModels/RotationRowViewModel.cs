using System.Windows;
using System.Windows.Media;
using S2x.ServerManager.Models;
using S2x.ServerManager.Views;

namespace S2x.ServerManager.ViewModels
{
    /// <summary>One line of the rotation: a map, the mode it runs, and where it sits in the order.</summary>
    internal sealed class RotationRowViewModel : Observable
    {
        private bool _isDropTarget;
        private bool _isDragging;

        public RotationRowViewModel(EditorViewModel editor, RotationEntry entry)
        {
            Editor = editor;
            Entry = entry;
            TopCommand = new RelayCommand(() => editor.Move(Index, 0));
            UpCommand = new RelayCommand(() => editor.Move(Index, Index - 1));
            DownCommand = new RelayCommand(() => editor.Move(Index, Index + 1));
            RemoveCommand = new RelayCommand(() => editor.Remove(this));
        }

        public EditorViewModel Editor { get; private set; }
        public RotationEntry Entry { get; private set; }
        public int Index { get; set; }

        public RelayCommand TopCommand { get; private set; }
        public RelayCommand UpCommand { get; private set; }
        public RelayCommand DownCommand { get; private set; }
        public RelayCommand RemoveCommand { get; private set; }

        public void Refresh() { RaiseAll(); }

        public string Number { get { return (Index + 1).ToString("00"); } }
        public string MapName { get { return GameData.MapName(Entry.Map); } }
        public string Pack { get { return GameData.MapPack(Entry.Map); } }
        public Visibility PackVisibility { get { return Pack == null ? Visibility.Collapsed : Visibility.Visible; } }
        public string GametypeCaps { get { return Editor.IsZombies ? "ZM" : GameData.GametypeShort(Entry.Gametype); } }
        public string Line { get { return "gametype " + Entry.Gametype + " map " + Entry.Map; } }

        /// <summary>The score limit this line will play to, from 03. Zombies has none.</summary>
        public string Limit
        {
            get
            {
                int limit;
                if (Editor.IsZombies || !Editor.ScoreLimitFor(Entry.Gametype, out limit)) return "";
                return limit + " pts";
            }
        }

        /// <summary>The line the server is on right now, when it is this server's own rotation.</summary>
        public bool IsNow { get { return Editor.NowIndex == Index; } }
        public Visibility NowVisibility { get { return IsNow ? Visibility.Visible : Visibility.Collapsed; } }

        public Brush NumberBackground { get { return IsNow ? Palette.TagBg : Palette.Panel; } }
        public Brush NumberForeground { get { return IsNow ? Palette.Accent : Palette.Muted; } }
        public Brush NumberEdge { get { return IsNow ? Palette.TagEdge : Palette.Edge; } }

        public Brush Background { get { return _isDropTarget ? Palette.Field : Palette.Bar; } }
        public Brush Edge { get { return _isDropTarget ? Palette.EdgeHot : Palette.Line; } }
        public double Opacity { get { return _isDragging ? 0.4 : 1.0; } }

        public bool IsDropTarget
        {
            get { return _isDropTarget; }
            set { if (Set(ref _isDropTarget, value)) { Raise("Background"); Raise("Edge"); } }
        }

        public bool IsDragging
        {
            get { return _isDragging; }
            set { if (Set(ref _isDragging, value)) Raise("Opacity"); }
        }
    }
}
