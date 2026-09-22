using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media;
using System.Windows.Threading;
using S2x.ServerManager.Models;
using S2x.ServerManager.Services;
using S2x.ServerManager.Views;

namespace S2x.ServerManager.ViewModels
{
    /// <summary>One line of the log, with the colour its text earned.</summary>
    internal sealed class LogLine
    {
        public string Text { get; set; }
        public Brush Fill { get; set; }
    }

    /// <summary>
    /// The console drawer: the tail of one server's log file. A drawer at the bottom of the
    /// editor and over the fleet's cards, both looking at this one view model, so a server's
    /// console is the same console whichever way it was opened.
    /// </summary>
    internal sealed class ConsoleViewModel : Observable
    {
        /// <summary>What the drawer keeps. The file keeps everything; this is a window on it.</summary>
        public const int MaxLines = 500;

        /// <summary>
        /// How long a server gets to write its own log before the drawer gives up and reads the
        /// one every server shares. g_consoleLog has not been confirmed on a live server yet, so
        /// the drawer has to show something either way and say which it is showing.
        /// </summary>
        public const int FallbackSeconds = 30;

        private const double MinHeight = 150;
        private const double MaxHeight = 560;

        private readonly FleetViewModel _fleet;
        private readonly DispatcherTimer _timer;
        private readonly List<LogLine> _all = new List<LogLine>();

        private ServerCardViewModel _card;
        private LogTail _tail;
        private string _fixedPath;          // the screenshot switch's own log
        private DateTime _watchingSince;
        private bool _open;
        private bool _paused;
        private bool _follow = true;
        private bool _shared;
        private bool _reading;
        private int _held;
        private double _height = 220;
        private string _filter = "";

        public ConsoleViewModel(FleetViewModel fleet)
        {
            _fleet = fleet;
            Lines = new ObservableCollection<LogLine>();
            CloseCommand = new RelayCommand(Close);
            PauseCommand = new RelayCommand(TogglePause);
            FollowCommand = new RelayCommand(ToggleFollow);
            CopyCommand = new RelayCommand(Copy);
            _timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
            _timer.Tick += (s, e) => Poll();
        }

        public ObservableCollection<LogLine> Lines { get; private set; }

        public RelayCommand CloseCommand { get; private set; }
        public RelayCommand PauseCommand { get; private set; }
        public RelayCommand FollowCommand { get; private set; }
        public RelayCommand CopyCommand { get; private set; }

        /// <summary>The drawer on screen focuses its filter box. Ctrl+L asks for it.</summary>
        public event Action FilterFocusRequested;

        /// <summary>Lines arrived: the drawer scrolls to the end if it is following.</summary>
        public event Action LinesAppended;

        public ServerCardViewModel Card { get { return _card; } }

        // ── opening and closing ───────────────────────────────────────────────────
        /// <summary>The CONSOLE button: this server's log, or shut if it is already showing.</summary>
        public void Toggle(ServerCardViewModel card)
        {
            if (card == null) return;
            if (_open && _card == card) { Close(); return; }
            Watch(card);
        }

        public void Watch(ServerCardViewModel card)
        {
            if (card == null) return;
            if (_open && _card == card) return;
            _card = card;
            Restart();
            _open = true;
            _timer.Start();
            Placement();
            Raise("ServerName"); Raise("DotBrush");
            Poll();
        }

        /// <summary>The roster's pane and the drawer show one server between them.</summary>
        public void Follow(ServerCardViewModel card)
        {
            if (_open && card != null && card != _card) Watch(card);
        }

        public void Close()
        {
            if (!_open) return;
            _open = false;
            _timer.Stop();
            Placement();
        }

        /// <summary>The screen or the view changed under the drawer, so it moves with them.</summary>
        public void Placement()
        {
            Raise("CardsVisibility"); Raise("EditorVisibility"); Raise("IsOpen");
        }

        public void RequestFilterFocus()
        {
            var handler = FilterFocusRequested;
            if (handler != null) handler();
        }

        private void Restart()
        {
            _all.Clear();
            Lines.Clear();
            _held = 0;
            _paused = false;
            _shared = false;
            _tail = null;
            _watchingSince = DateTime.Now;
            RaiseChrome();
        }

        // ── where the drawer sits ─────────────────────────────────────────────────
        public bool IsOpen { get { return _open; } }

        /// <summary>Over the fleet's cards, the mockup's home drawer.</summary>
        public Visibility CardsVisibility
        {
            get { return Show(_open && _fleet.OnCards); }
        }

        /// <summary>Under the editor, above its footer: the editor screen and the roster pane.</summary>
        public Visibility EditorVisibility
        {
            get { return Show(_open && !_fleet.OnCards); }
        }

        public double Height
        {
            get { return _height; }
            set { Set(ref _height, Math.Max(MinHeight, Math.Min(MaxHeight, value))); }
        }

        // ── header ────────────────────────────────────────────────────────────────
        public string ServerName { get { return _card == null ? "" : _card.Name; } }
        public Brush DotBrush { get { return _card == null ? Palette.Off : _card.DotBrush; } }

        public string LogFile
        {
            get { return _tail == null ? "" : _tail.Path; }
        }

        /// <summary>
        /// Said out loud when the drawer is not reading what it asked for: the per-server log
        /// never appeared, so this is the log every server on the box writes to at once.
        /// </summary>
        public string SharedNote
        {
            get { return "shared log; per-server log did not appear"; }
        }

        public Visibility SharedVisibility { get { return Show(_shared); } }

        public string PauseLabel { get { return _paused ? "PAUSED" : "PAUSE"; } }
        public Brush PauseBackground { get { return _paused ? Palette.TagBg : Palette.Transparent; } }
        public Brush PauseForeground { get { return _paused ? Palette.Accent : Palette.Ink; } }
        public Brush FollowBackground { get { return _follow ? Palette.TagBg : Palette.Transparent; } }
        public Brush FollowForeground { get { return _follow ? Palette.Accent : Palette.Ink; } }
        public bool IsFollowing { get { return _follow && !_paused; } }

        public string HeldNote
        {
            get { return "— paused · " + _held + (_held == 1 ? " new line held —" : " new lines held —"); }
        }

        public Visibility HeldVisibility { get { return Show(_paused); } }

        public string Filter
        {
            get { return _filter; }
            set
            {
                if (!Set(ref _filter, value ?? "")) return;
                Raise("FilterPlaceholderVisibility");
                Rebuild();
            }
        }

        public Visibility FilterPlaceholderVisibility { get { return Show(_filter.Length == 0); } }

        // ── the poll round ────────────────────────────────────────────────────────
        /// <summary>
        /// A second of the file at a time, read off the UI thread: the drawer must not hold the
        /// window up while a server writes a megabyte into its log.
        /// </summary>
        private async void Poll()
        {
            if (!_open || _reading) return;
            _reading = true;
            try
            {
                var wanted = WantedPath();
                if (wanted == null) return;
                var shared = _fixedPath == null && _card != null &&
                             string.Equals(wanted, GameFolder.SharedLogPath(_fleet.GameDir),
                                           StringComparison.OrdinalIgnoreCase);

                if (_tail == null || !string.Equals(_tail.Path, wanted, StringComparison.OrdinalIgnoreCase))
                {
                    if (_tail != null) Append(Marker("— now reading " + wanted + " —"));
                    _tail = new LogTail(wanted);
                    Raise("LogFile");
                }
                if (shared != _shared) { _shared = shared; Raise("SharedVisibility"); }

                var tail = _tail;
                var chunk = await Task.Run(() => tail.Read());
                if (tail != _tail) return;   // the drawer moved on while the read was running
                Apply(chunk);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine("console read failed: " + ex);
            }
            finally
            {
                _reading = false;
            }
        }

        /// <summary>
        /// The server's own log if it is there, and the shared one once it is clear it is not
        /// coming: thirty seconds from the later of the server starting and this drawer opening.
        /// </summary>
        private string WantedPath()
        {
            if (_fixedPath != null) return _fixedPath;
            if (_card == null || _fleet.IsDemo) return null;

            var own = GameFolder.ServerLogPath(_fleet.GameDir, _card.Preset.Port);
            if (File.Exists(own)) return own;

            var start = _card.State.ProcessStart;
            var since = start.HasValue && start.Value > _watchingSince ? start.Value : _watchingSince;
            if ((DateTime.Now - since).TotalSeconds < FallbackSeconds) return own;
            return GameFolder.SharedLogPath(_fleet.GameDir);
        }

        private void Apply(LogChunk chunk)
        {
            if (chunk.Cleared) Append(Marker("— cleared · the server started again —"));
            if (chunk.Lines.Count == 0) return;

            if (_paused) { _held += chunk.Lines.Count; Raise("HeldNote"); }
            foreach (var text in chunk.Lines) Append(new LogLine { Text = text, Fill = Colour(text) });
            if (!_paused)
            {
                var handler = LinesAppended;
                if (handler != null) handler();
            }
        }

        private void Append(LogLine line)
        {
            _all.Add(line);
            if (!_paused && Matches(line)) Lines.Add(line);
            while (_all.Count > MaxLines)
            {
                var dropped = _all[0];
                _all.RemoveAt(0);
                if (Lines.Count > 0 && ReferenceEquals(Lines[0], dropped)) Lines.RemoveAt(0);
            }
        }

        private void Rebuild()
        {
            Lines.Clear();
            foreach (var line in _all) if (Matches(line)) Lines.Add(line);
            var handler = LinesAppended;
            if (handler != null) handler();
        }

        private bool Matches(LogLine line)
        {
            if (_filter.Length == 0) return true;
            return line.Text.IndexOf(_filter, StringComparison.OrdinalIgnoreCase) >= 0;
        }

        private static LogLine Marker(string text)
        {
            return new LogLine { Text = text, Fill = Palette.Accent };
        }

        /// <summary>
        /// What the line says about itself. The fork writes plain text with no level on it, so
        /// this is the words, and anything it does not recognise stays the ordinary colour.
        /// </summary>
        private static Brush Colour(string text)
        {
            if (Holds(text, "fatal") || Holds(text, "exception") || Holds(text, "error")) return Palette.Danger;
            if (Holds(text, "warning") || Holds(text, "warn:")) return Palette.Accent;
            if (text.StartsWith("set ", StringComparison.OrdinalIgnoreCase) ||
                text.StartsWith("[S2x]", StringComparison.OrdinalIgnoreCase)) return Palette.Ink;
            return Palette.Label;
        }

        private static bool Holds(string text, string word)
        {
            return text.IndexOf(word, StringComparison.OrdinalIgnoreCase) >= 0;
        }

        // ── buttons ───────────────────────────────────────────────────────────────
        private void TogglePause()
        {
            _paused = !_paused;
            if (!_paused)
            {
                _held = 0;
                Rebuild();
            }
            RaiseChrome();
        }

        private void ToggleFollow()
        {
            _follow = !_follow;
            RaiseChrome();
            if (_follow)
            {
                var handler = LinesAppended;
                if (handler != null) handler();
            }
        }

        private void Copy()
        {
            try
            {
                var text = new StringBuilder();
                foreach (var line in Lines) text.AppendLine(line.Text);
                if (text.Length == 0) { _fleet.Toast("Nothing in the console to copy yet."); return; }
                Clipboard.SetText(text.ToString());
                _fleet.Toast("Copied " + Lines.Count + (Lines.Count == 1 ? " line" : " lines"));
            }
            catch (Exception ex)
            {
                _fleet.Toast(ex.Message);
            }
        }

        private void RaiseChrome()
        {
            Raise("PauseLabel"); Raise("PauseBackground"); Raise("PauseForeground");
            Raise("FollowBackground"); Raise("FollowForeground"); Raise("IsFollowing");
            Raise("HeldNote"); Raise("HeldVisibility"); Raise("SharedVisibility"); Raise("LogFile");
        }

        private static Visibility Show(bool on) { return on ? Visibility.Visible : Visibility.Collapsed; }

        // ── --screenshot-console ──────────────────────────────────────────────────
        /// <summary>
        /// A render switch has nobody to wait a second for the first poll, and no server: it
        /// points the drawer at a file it wrote itself and reads it here and now.
        /// </summary>
        public void ShowFile(ServerCardViewModel card, string path)
        {
            _card = card;
            Restart();
            _fixedPath = path;
            _open = true;
            _tail = new LogTail(path);
            Apply(_tail.Read());
            Placement();
            Raise("ServerName"); Raise("DotBrush"); Raise("LogFile");
        }
    }
}
