using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Media;
using S2x.ServerManager.Models;
using S2x.ServerManager.Services;
using S2x.ServerManager.Views;

namespace S2x.ServerManager.ViewModels
{
    internal sealed class SwatchViewModel
    {
        public Brush Fill { get; set; }
        public Brush Stroke { get; set; }
        public string Tip { get; set; }
        public RelayCommand InsertCommand { get; set; }
    }

    /// <summary>One button of a segmented control: mode, name pool, difficulty.</summary>
    internal sealed class SegmentOption
    {
        public string Label { get; set; }
        public Brush Background { get; set; }
        public Brush Foreground { get; set; }
        public RelayCommand PickCommand { get; set; }
    }

    internal sealed class PickOption
    {
        public string Key { get; set; }
        public string Label { get; set; }

        /// <summary>The closed ComboBox shows the item itself, not its template.</summary>
        public override string ToString() { return Label; }
    }

    internal sealed class PipViewModel
    {
        public Brush Fill { get; set; }
        public Brush Stroke { get; set; }
    }

    /// <summary>A score limit box for one mode in the rotation.</summary>
    internal sealed class ScoreRowViewModel : Observable
    {
        private readonly EditorViewModel _editor;
        private string _text;

        public ScoreRowViewModel(EditorViewModel editor, string gametype, int value)
        {
            _editor = editor;
            Gametype = gametype;
            Label = GameData.GametypeName(gametype);
            _text = value.ToString();
        }

        public string Gametype { get; private set; }
        public string Label { get; private set; }

        public string ValueText
        {
            get { return _text; }
            set
            {
                if (!Set(ref _text, EditorViewModel.Digits(value, 3))) return;
                _editor.ScoreLimitChanged(this);
            }
        }

        public int Value
        {
            get
            {
                int parsed;
                if (!int.TryParse(_text, out parsed)) return GameData.DefaultScoreLimits.First(p => p.Key == Gametype).Value;
                return Math.Max(1, Math.Min(999, parsed));
            }
        }
    }

    /// <summary>
    /// The editor screen: one preset, edited in full and written back as a file. Everything here
    /// applies at the next launch, so the model is stop, change, start; nothing is sent to a
    /// server that is already up.
    /// </summary>
    internal sealed class EditorViewModel : Observable
    {
        private readonly FleetViewModel _fleet;
        private ServerPreset _preset;
        private bool _portLocked;
        private bool _fileChanged;

        private string _name;
        private bool _isZombies;
        private string _portText;
        private bool _shuffleOnLaunch;
        private bool _singleRoundDom;
        private int _botFill;
        private string _botNames;
        private string _botDifficulty;
        private string _capText;
        private string _minText;
        private string _delayText;
        private bool _advertise;
        private string _extraText;
        private string _saved;
        private RotationRowViewModel _selectedRow;
        private PickOption _pickedMap;
        private PickOption _pickedGametype;
        private ServerState _state = new ServerState();

        public EditorViewModel(FleetViewModel fleet, ServerPreset preset, bool isNew)
        {
            _fleet = fleet;
            _preset = preset;
            IsNew = isNew;

            _name = preset.ServerName ?? "";
            _isZombies = preset.IsZombies;
            _portText = preset.Port.ToString();
            _shuffleOnLaunch = preset.ShuffleOnLaunch;
            _singleRoundDom = preset.SingleRoundDom;
            _botFill = preset.BotFill;
            _botNames = preset.BotNames;
            _botDifficulty = preset.BotDifficulty;
            _capText = preset.MaxPlayers.ToString();
            _minText = preset.MinPlayers.ToString();
            _delayText = preset.StartDelay.ToString();
            _advertise = preset.Advertise;
            _extraText = string.Join(Environment.NewLine, preset.ExtraLines);
            foreach (var pair in GameData.DefaultScoreLimits)
            {
                int value;
                if (!preset.ScoreLimits.TryGetValue(pair.Key, out value)) value = pair.Value;
                _scores[pair.Key] = value;
            }

            Rotation = new ObservableCollection<RotationRowViewModel>();
            // Rows edit their own copies of the entries, keys this app does not know included.
            foreach (var entry in preset.Rotation) Rotation.Add(new RotationRowViewModel(this, entry.Copy()));

            Swatches = BuildSwatches();
            BuildPickers();
            SaveCommand = new RelayCommand(() => Save());
            SaveAsCommand = new RelayCommand(SaveAs);
            AddCommand = new RelayCommand(Add);
            RandomizeCommand = new RelayCommand(Randomize);
            ClearCommand = new RelayCommand(ClearRotation);
            SetMultiplayerCommand = new RelayCommand(() => SetMode(false));
            SetZombiesCommand = new RelayCommand(() => SetMode(true));
            LaunchCommand = new RelayCommand(Launch);
            // Stop and Restart belong to the process, which is on the port this server was saved
            // with, never on whatever is being typed into the port box.
            StopCommand = new RelayCommand(() => _fleet.StopPort(OwnedPort));
            RestartCommand = new RelayCommand(Restart);
            BackCommand = new RelayCommand(() => _fleet.ShowFleet());
            ConsoleCommand = new RelayCommand(() => _fleet.ToggleConsole(_preset));
            DeleteCommand = new RelayCommand(() => _fleet.DeletePreset(this));

            Renumber();
            _saved = isNew ? "" : Snapshot();
            RefreshState();
        }

        private readonly Dictionary<string, int> _scores = new Dictionary<string, int>(StringComparer.Ordinal);

        public bool IsNew { get; private set; }
        public FleetViewModel Fleet { get { return _fleet; } }
        public ServerPreset Preset { get { return _preset; } }
        public ObservableCollection<RotationRowViewModel> Rotation { get; private set; }
        public List<SwatchViewModel> Swatches { get; private set; }

        public RelayCommand SaveCommand { get; private set; }
        public RelayCommand SaveAsCommand { get; private set; }
        public RelayCommand AddCommand { get; private set; }
        public RelayCommand RandomizeCommand { get; private set; }
        public RelayCommand ClearCommand { get; private set; }
        public RelayCommand SetMultiplayerCommand { get; private set; }
        public RelayCommand SetZombiesCommand { get; private set; }
        public RelayCommand LaunchCommand { get; private set; }
        public RelayCommand StopCommand { get; private set; }
        public RelayCommand RestartCommand { get; private set; }
        public RelayCommand BackCommand { get; private set; }
        public RelayCommand ConsoleCommand { get; private set; }
        public RelayCommand DeleteCommand { get; private set; }

        /// <summary>The view owns the caret; the swatches drop a colour code where it sits.</summary>
        public int CaretIndex { get; set; }
        public event Action<int> CaretSet;

        // ── 01 name, mode, port ───────────────────────────────────────────────────
        public string Name
        {
            get { return _name; }
            set { if (Set(ref _name, value ?? "")) Touched(); }
        }

        public string PlainName
        {
            get
            {
                var name = GameData.StripColorCodes(_name).Trim();
                return name.Length > 0 ? name : (_preset.FileName ?? "New server");
            }
        }

        public string FileName { get { return _preset.FileName; } }

        public bool IsZombies { get { return _isZombies; } }
        public bool IsMultiplayer { get { return !_isZombies; } }
        public Visibility MultiplayerVisibility { get { return _isZombies ? Visibility.Collapsed : Visibility.Visible; } }
        public Brush MpBackground { get { return _isZombies ? Palette.Transparent : Palette.Accent; } }
        public Brush MpForeground { get { return _isZombies ? Palette.Muted : Palette.Bar; } }
        public Brush ZmBackground { get { return _isZombies ? Palette.Accent : Palette.Transparent; } }
        public Brush ZmForeground { get { return _isZombies ? Palette.Bar : Palette.Muted; } }

        /// <summary>
        /// The port the running process is on: the one the preset was saved with. The box above
        /// it is the port the next launch will use, and the two are only the same once saved.
        /// </summary>
        public int OwnedPort { get { return _preset.Port; } }

        public string PortText
        {
            get { return _portText; }
            set
            {
                var digits = Digits(value, 5);
                if (digits == _portText) return;
                if (!IsStopped && digits != OwnedPort.ToString())
                {
                    // A server is running on the port this preset holds. Moving it now would
                    // point Stop and Restart at whatever else is on the new one.
                    _portLocked = true;
                    Raise("PortText"); Raise("PortWarning"); Raise("PortWarningVisibility");
                    return;
                }
                _portLocked = false;
                _portText = digits;
                Raise("PortText");
                Touched();
                Raise("PortWarning"); Raise("PortWarningVisibility"); Raise("CfgPath");
            }
        }

        public int Port
        {
            get
            {
                int parsed;
                if (!int.TryParse(_portText, out parsed)) return 27016;
                return Math.Max(1024, Math.Min(65535, parsed));
            }
        }

        /// <summary>Two servers on one port is one server: the second never gets the socket.</summary>
        public string PortWarning
        {
            get
            {
                if (_portLocked) return "Stop the server before changing its port.";
                int parsed;
                if (!int.TryParse(_portText, out parsed) || parsed < 1024 || parsed > 65535)
                    return "A port between 1024 and 65535.";
                var others = _fleet.PresetsOnPort(parsed, _preset);
                if (others == 0) return "";
                return "shares :" + parsed + " with " + others + (others == 1 ? " preset" : " presets");
            }
        }

        public Visibility PortWarningVisibility { get { return PortWarning.Length == 0 ? Visibility.Collapsed : Visibility.Visible; } }

        // ── 02 rotation ───────────────────────────────────────────────────────────
        public string RotationTitle { get { return _isZombies ? "ZONE ROTATION" : "MAP ROTATION"; } }
        public string RotationCount { get { return Rotation.Count + (Rotation.Count == 1 ? " entry" : " entries"); } }

        public string RotationMeta
        {
            get
            {
                if (Rotation.Count == 0) return "";
                var modes = Math.Max(1, Modes().Count);
                var dlc = Rotation.Count(r => r.Pack != null);
                var text = modes + (modes == 1 ? " mode" : " modes");
                if (dlc > 0) text += " " + GameData.MiddleDot + " " + dlc + " need DLC";
                if (_shuffleOnLaunch) text += " " + GameData.MiddleDot + " shuffled at launch";
                return text;
            }
        }

        public Visibility EmptyRotationVisibility { get { return Rotation.Count == 0 ? Visibility.Visible : Visibility.Collapsed; } }

        public bool ShuffleOnLaunch
        {
            get { return _shuffleOnLaunch; }
            set { if (Set(ref _shuffleOnLaunch, value)) { Raise("RotationMeta"); Touched(); } }
        }

        public List<PickOption> MapOptions { get; private set; }
        public List<PickOption> GametypeOptions { get; private set; }

        public PickOption PickedMap
        {
            get { return _pickedMap; }
            set { Set(ref _pickedMap, value); }
        }

        public PickOption PickedGametype
        {
            get { return _pickedGametype; }
            set { Set(ref _pickedGametype, value); }
        }

        public RotationRowViewModel SelectedRow
        {
            get { return _selectedRow; }
            set { Set(ref _selectedRow, value); }
        }

        // ── 03 match rules ────────────────────────────────────────────────────────
        public List<ScoreRowViewModel> ScoreRows { get; private set; }

        public string ScoreSummary
        {
            get
            {
                var modes = Modes();
                if (modes.Count == 0) return "no modes yet";
                var changed = modes.Count(m => _scores[m] != GameData.DefaultScoreLimits.First(p => p.Key == m).Value);
                return changed == 0 ? "defaults" : changed + " changed";
            }
        }

        public Visibility DomVisibility { get { return Modes().Contains("dom") ? Visibility.Visible : Visibility.Collapsed; } }

        public bool SingleRoundDom
        {
            get { return _singleRoundDom; }
            set { if (Set(ref _singleRoundDom, value)) Touched(); }
        }

        public bool ScoreLimitFor(string gametype, out int limit)
        {
            return _scores.TryGetValue(gametype ?? "", out limit);
        }

        public void ScoreLimitChanged(ScoreRowViewModel row)
        {
            _scores[row.Gametype] = row.Value;
            foreach (var line in Rotation) line.Refresh();
            Raise("ScoreSummary");
            Touched();
        }

        // ── 04 bots ───────────────────────────────────────────────────────────────
        public int BotFill
        {
            get { return _botFill; }
            set
            {
                var wanted = Math.Max(0, Math.Min(Cap, value));
                if (!Set(ref _botFill, wanted)) return;
                Raise("BotPips"); Raise("BotSummary");
                Touched();
            }
        }

        public List<PipViewModel> BotPips
        {
            get
            {
                var pips = new List<PipViewModel>(Cap);
                for (int i = 0; i < Cap; i++)
                    pips.Add(i < _botFill
                        ? new PipViewModel { Fill = Palette.EdgeHot, Stroke = Palette.EdgeHot }
                        : new PipViewModel { Fill = Palette.Transparent, Stroke = Palette.Edge });
                return pips;
            }
        }

        public string BotSummary
        {
            get { return _botFill + " bots " + GameData.MiddleDot + " " + _botNames + " " + GameData.MiddleDot + " " + _botDifficulty; }
        }

        public List<SegmentOption> PoolOptions
        {
            get
            {
                return GameData.BotNamePools.Select(pool => Segment(pool.ToUpperInvariant(), _botNames == pool,
                    () => { _botNames = pool; Raise("PoolOptions"); Raise("BotSummary"); Touched(); })).ToList();
            }
        }

        public List<SegmentOption> DifficultyOptions
        {
            get
            {
                return GameData.BotDifficulties.Select(level => Segment(level.ToUpperInvariant(), _botDifficulty == level,
                    () => { _botDifficulty = level; Raise("DifficultyOptions"); Raise("BotSummary"); Touched(); })).ToList();
            }
        }

        // ── 05 lobby ──────────────────────────────────────────────────────────────
        public int Cap
        {
            get
            {
                int parsed;
                var ceiling = ServerPreset.CapCeiling(_isZombies);
                if (!int.TryParse(_capText, out parsed)) return ceiling;
                return Math.Max(1, Math.Min(ceiling, parsed));
            }
        }

        public string CapText
        {
            get { return _capText; }
            set
            {
                var ceiling = ServerPreset.CapCeiling(_isZombies);
                var text = Digits(value, 2);
                int parsed;
                if (int.TryParse(text, out parsed)) text = Math.Max(1, Math.Min(ceiling, parsed)).ToString();
                if (!Set(ref _capText, text)) return;
                // A cap the party cannot hold would only be clamped by the game; clamp what
                // depends on it here instead, so the numbers on screen agree. The minimum's own
                // text has to move too, or the box keeps a number the server will not use.
                if (_botFill > Cap) { _botFill = Cap; Raise("BotFill"); }
                ClampMinimum();
                Raise("Cap"); Raise("BotPips"); Raise("BotSummary"); Raise("LobbySummary");
                Touched();
            }
        }

        public string CapRange { get { return _isZombies ? "1-4" : "1-18"; } }

        private void ClampMinimum()
        {
            int typed;
            if (!int.TryParse(_minText, out typed) || typed <= Cap) return;
            _minText = Cap.ToString();
            Raise("MinText");
        }

        public int MinPlayers
        {
            get
            {
                int parsed;
                if (!int.TryParse(_minText, out parsed)) return 1;
                return Math.Max(1, Math.Min(Cap, parsed));
            }
        }

        public string MinText
        {
            get { return _minText; }
            set
            {
                var text = Digits(value, 2);
                int parsed;
                if (int.TryParse(text, out parsed)) text = Math.Max(1, Math.Min(Cap, parsed)).ToString();
                if (!Set(ref _minText, text)) return;
                Raise("LobbySummary");
                Touched();
            }
        }

        public int StartDelay
        {
            get
            {
                int parsed;
                if (!int.TryParse(_delayText, out parsed)) return 0;
                return Math.Max(0, Math.Min(ServerPreset.MaxStartDelay, parsed));
            }
        }

        public string DelayText
        {
            get { return _delayText; }
            set
            {
                var text = Digits(value, 3);
                int parsed;
                if (int.TryParse(text, out parsed)) text = Math.Min(ServerPreset.MaxStartDelay, parsed).ToString();
                if (!Set(ref _delayText, text)) return;
                Raise("LobbySummary");
                Touched();
            }
        }

        public string LobbySummary
        {
            get { return "cap " + Cap + " " + GameData.MiddleDot + " min " + MinPlayers + " " + GameData.MiddleDot + " " + StartDelay + " s"; }
        }

        public string LobbyNumber { get { return _isZombies ? "03" : "05"; } }
        public string VisibilityNumber { get { return _isZombies ? "04" : "06"; } }
        public string AdvancedNumber { get { return _isZombies ? "05" : "07"; } }

        // ── 06 visibility ─────────────────────────────────────────────────────────
        public bool Advertise
        {
            get { return _advertise; }
            set
            {
                if (!Set(ref _advertise, value)) return;
                Raise("VisibilitySummary"); Raise("VisibilityBrush"); Raise("VisibilityNote");
                Touched();
            }
        }

        public string VisibilitySummary { get { return _advertise ? "master list" : "LAN only"; } }
        public Brush VisibilityBrush { get { return _advertise ? Palette.Ok : Palette.Muted; } }

        public string VisibilityNote
        {
            get
            {
                return _advertise
                    ? "Shows in everyone's server browser. Forward the UDP port on your router."
                    : "Off means LAN only: nobody outside your network will see it.";
            }
        }

        // ── 07 advanced ───────────────────────────────────────────────────────────
        public string ExtraText
        {
            get { return _extraText; }
            set
            {
                if (!Set(ref _extraText, value ?? "")) return;
                Raise("AdvancedSummary"); Raise("ExtraPlaceholderVisibility");
                Touched();
            }
        }

        /// <summary>WPF has no placeholder, so the hint sits behind the box until it is used.</summary>
        public Visibility ExtraPlaceholderVisibility
        {
            get { return string.IsNullOrEmpty(_extraText) ? Visibility.Visible : Visibility.Collapsed; }
        }

        public List<string> ExtraLines
        {
            get
            {
                // Passed through as written: blank lines go, every other line stays as typed.
                return (_extraText ?? "")
                    .Split(new[] { "\r\n", "\n" }, StringSplitOptions.None)
                    .Where(line => !string.IsNullOrWhiteSpace(line))
                    .ToList();
            }
        }

        public string AdvancedSummary
        {
            get
            {
                var count = ExtraLines.Count;
                return count == 0 ? "none" : count + (count == 1 ? " line" : " lines");
            }
        }

        // ── footer ────────────────────────────────────────────────────────────────
        public string CfgPath { get { return GameFolder.CfgPath(_fleet.GameDir, Port); } }

        public string StateCaps
        {
            get
            {
                switch (_state.Status)
                {
                    case ServerStatus.Running: return "RUNNING";
                    case ServerStatus.Starting: return "STARTING";
                    case ServerStatus.NotAnswering: return "NOT ANSWERING";
                    case ServerStatus.Crashed: return "PROCESS GONE";
                    default: return "STOPPED";
                }
            }
        }

        public Brush StateBrush
        {
            get
            {
                switch (_state.Status)
                {
                    case ServerStatus.Running: return Palette.Ok;
                    case ServerStatus.Starting:
                    case ServerStatus.NotAnswering: return Palette.Accent;
                    case ServerStatus.Crashed: return Palette.Danger;
                    default: return Palette.Muted;
                }
            }
        }

        public Brush DotBrush { get { return _state.Status == ServerStatus.Stopped ? Palette.Off : StateBrush; } }

        public string StatusSub
        {
            get
            {
                if (_state.Status == ServerStatus.Stopped) return "no process";
                if (_state.Status == ServerStatus.Crashed) return "PID " + _state.Pid + " exited " + GameData.MiddleDot + " 127.0.0.1:" + OwnedPort;
                var live = "PID " + _state.Pid + " " + GameData.MiddleDot + " 127.0.0.1:" + OwnedPort;
                if (_state.Status == ServerStatus.Running)
                    live += " " + GameData.MiddleDot + " " + _state.Humans + " humans " + GameData.MiddleDot + " " +
                            _state.Bots + " bots " + GameData.MiddleDot + " up " + ServerState.FormatSpan(_state.Uptime);
                return live;
            }
        }

        /// <summary>Launch when there is nothing to stop; Stop and Restart while a process is alive.</summary>
        public bool IsStopped { get { return _state.Status == ServerStatus.Stopped || _state.Status == ServerStatus.Crashed; } }
        public Visibility LaunchVisibility { get { return IsStopped ? Visibility.Visible : Visibility.Collapsed; } }
        public Visibility RunningVisibility { get { return IsStopped ? Visibility.Collapsed : Visibility.Visible; } }

        /// <summary>
        /// Delete takes the preset's file away, so it is only offered with nothing running on its
        /// port. A new server has no file yet: leaving the editor already asks about that one.
        /// </summary>
        public Visibility DeleteVisibility { get { return IsStopped && !IsNew ? Visibility.Visible : Visibility.Collapsed; } }

        /// <summary>HIDE on the card wrote this preset's file; the editor writes the same key.</summary>
        public void SetHidden(bool hidden) { _preset.Hidden = hidden; }

        public bool IsDirty { get { return Snapshot() != _saved; } }
        public Visibility DirtyVisibility { get { return IsDirty ? Visibility.Visible : Visibility.Collapsed; } }

        /// <summary>
        /// Somebody else wrote this preset's file while it was being edited here. The draft is
        /// kept, because it is work nobody else can recover, and the host is told.
        /// </summary>
        public bool FileChanged
        {
            get { return _fileChanged; }
            set { if (Set(ref _fileChanged, value)) Raise("FileChangedVisibility"); }
        }

        public Visibility FileChangedVisibility { get { return _fileChanged ? Visibility.Visible : Visibility.Collapsed; } }

        /// <summary>Where the running server is in this rotation, or -1.</summary>
        public int NowIndex
        {
            get
            {
                if (!_state.IsLive || _state.MapKey == null) return -1;
                for (int i = 0; i < Rotation.Count; i++)
                {
                    var entry = Rotation[i].Entry;
                    if (entry.Map == _state.MapKey && entry.Gametype == _state.GametypeKey) return i;
                }
                return -1;
            }
        }

        /// <summary>A poll round landed: the state line, and the NOW marker, follow it.</summary>
        public void RefreshState()
        {
            // The state belongs to the process, so it is read on the owned port. Reading it on
            // the port being typed would borrow another server's players and map.
            _state = _fleet.StateFor(OwnedPort);
            if (IsStopped && _portLocked) { _portLocked = false; Raise("PortWarning"); Raise("PortWarningVisibility"); }
            Raise("StateCaps"); Raise("StateBrush"); Raise("DotBrush"); Raise("StatusSub");
            Raise("LaunchVisibility"); Raise("RunningVisibility"); Raise("DeleteVisibility");
            foreach (var row in Rotation) row.Refresh();
        }

        // ── rotation edits ────────────────────────────────────────────────────────
        public void Add()
        {
            if (PickedMap == null) return;
            var gametype = _isZombies ? "zombies" : (PickedGametype != null ? PickedGametype.Key : "war");
            Rotation.Add(new RotationRowViewModel(this, new RotationEntry { Map = PickedMap.Key, Gametype = gametype }));
            RotationChanged();
        }

        public void Remove(RotationRowViewModel row)
        {
            if (row == null) return;
            Rotation.Remove(row);
            RotationChanged();
        }

        public void RemoveSelected()
        {
            var row = SelectedRow;
            if (row == null) return;
            var index = Rotation.IndexOf(row);
            Rotation.Remove(row);
            RotationChanged();
            if (Rotation.Count > 0) SelectedRow = Rotation[Math.Min(index, Rotation.Count - 1)];
        }

        public void Move(int from, int to)
        {
            if (from < 0 || from >= Rotation.Count || to < 0 || to >= Rotation.Count || from == to) return;
            Rotation.Move(from, to);
            RotationChanged();
        }

        /// <summary>
        /// A drop: <paramref name="boundary"/> is the gap between rows the pointer was over, so
        /// dropping on the top half of a row puts the dragged line before it and the bottom half
        /// after it. Taking the row out first shifts every gap below it up by one.
        /// </summary>
        public void MoveTo(RotationRowViewModel row, int boundary)
        {
            var from = Rotation.IndexOf(row);
            if (from < 0) return;
            if (boundary > from) boundary--;
            boundary = Math.Max(0, Math.Min(Rotation.Count - 1, boundary));
            Move(from, boundary);
        }

        private void Randomize()
        {
            var order = Rotation.ToList();
            var random = new Random();
            for (int i = order.Count - 1; i > 0; i--)
            {
                var j = random.Next(i + 1);
                var hold = order[i]; order[i] = order[j]; order[j] = hold;
            }
            Rotation.Clear();
            foreach (var row in order) Rotation.Add(row);
            RotationChanged();
        }

        private void ClearRotation()
        {
            Rotation.Clear();
            RotationChanged();
        }

        private void RotationChanged()
        {
            Renumber();
            Raise("RotationCount"); Raise("RotationMeta"); Raise("EmptyRotationVisibility");
            Raise("ScoreRows"); Raise("ScoreSummary"); Raise("DomVisibility");
            Touched();
        }

        private void Renumber()
        {
            for (int i = 0; i < Rotation.Count; i++) { Rotation[i].Index = i; Rotation[i].Refresh(); }
            ScoreRows = Modes().Select(mode => new ScoreRowViewModel(this, mode, _scores[mode])).ToList();
        }

        private List<string> Modes()
        {
            var modes = new List<string>();
            if (_isZombies) return modes;
            foreach (var row in Rotation)
                if (_scores.ContainsKey(row.Entry.Gametype) && !modes.Contains(row.Entry.Gametype)) modes.Add(row.Entry.Gametype);
            return modes;
        }

        // ── mode ──────────────────────────────────────────────────────────────────
        /// <summary>What one mode was holding, so switching back does not cost the rotation.</summary>
        private sealed class ModeDraft
        {
            public List<RotationEntry> Rotation;
            public string CapText;
            public string MinText;
            public int BotFill;
        }

        private readonly Dictionary<bool, ModeDraft> _drafts = new Dictionary<bool, ModeDraft>();

        private void SetMode(bool zombies)
        {
            if (_isZombies == zombies) return;

            // Switch-Mode in the launcher puts the outgoing mode away and brings the incoming
            // one back. It keeps that in its own preset files; this keeps it in the editor, so
            // switching to look at Zombies does not throw a multiplayer rotation away.
            _drafts[_isZombies] = new ModeDraft
            {
                Rotation = Rotation.Select(r => r.Entry.Copy()).ToList(),
                CapText = _capText,
                MinText = _minText,
                BotFill = _botFill,
            };
            _isZombies = zombies;

            ModeDraft draft;
            var table = GameData.MapsFor(zombies).Select(p => p.Key).ToList();
            var entries = _drafts.TryGetValue(zombies, out draft)
                ? draft.Rotation
                : Rotation.Select(r => r.Entry).ToList();

            Rotation.Clear();
            foreach (var entry in entries)
            {
                // The map table decides: no multiplayer map is a Zombies zone, so a rotation
                // carried across a switch for the first time empties.
                if (!table.Contains(entry.Map)) continue;
                entry.Gametype = zombies ? "zombies" : (_scores.ContainsKey(entry.Gametype) ? entry.Gametype : "war");
                Rotation.Add(new RotationRowViewModel(this, entry));
            }

            var ceiling = ServerPreset.CapCeiling(zombies);
            _capText = draft != null ? draft.CapText : ceiling.ToString();
            _botFill = Math.Min(draft != null ? draft.BotFill : _botFill, Cap);
            _minText = draft != null ? draft.MinText : "1";
            ClampMinimum();

            BuildPickers();
            RotationChanged();
            RaiseAll();
        }

        private void BuildPickers()
        {
            MapOptions = GameData.MapsFor(_isZombies).Select(pair => new PickOption
            {
                Key = pair.Key,
                Label = GameData.MapPack(pair.Key) == null
                    ? pair.Value
                    : pair.Value + "  " + GameData.MiddleDot + "  " + GameData.MapPack(pair.Key),
            }).ToList();
            GametypeOptions = _isZombies
                ? new List<PickOption> { new PickOption { Key = "zombies", Label = "Zombies" } }
                : GameData.Gametypes.Select(pair => new PickOption { Key = pair.Key, Label = pair.Value }).ToList();
            _pickedMap = MapOptions.FirstOrDefault();
            _pickedGametype = GametypeOptions.FirstOrDefault();
            Raise("MapOptions"); Raise("GametypeOptions"); Raise("PickedMap"); Raise("PickedGametype");
        }

        private List<SwatchViewModel> BuildSwatches()
        {
            var order = new[] { '1', '2', '3', '4', '5', '6', '7', '0' };
            var names = new Dictionary<char, string>
            {
                { '1', "Red" }, { '2', "Green" }, { '3', "Yellow" }, { '4', "Blue" },
                { '5', "Cyan" }, { '6', "Magenta" }, { '7', "White" }, { '0', "Black" },
            };
            return order.Select(code => new SwatchViewModel
            {
                Fill = Palette.Frozen(GameData.ColorCodes[code]),
                Stroke = code == '0' ? Palette.EdgeHot : Palette.Frozen(GameData.ColorCodes[code]),
                Tip = names[code] + "  ^" + code,
                InsertCommand = new RelayCommand(() => Insert(code)),
            }).ToList();
        }

        private void Insert(char code)
        {
            var at = Math.Max(0, Math.Min(_name.Length, CaretIndex));
            Name = _name.Substring(0, at) + "^" + code + _name.Substring(at);
            CaretIndex = at + 2;
            var handler = CaretSet;
            if (handler != null) handler(CaretIndex);
        }

        private static SegmentOption Segment(string label, bool on, Action pick)
        {
            return new SegmentOption
            {
                Label = label,
                Background = on ? Palette.Accent : Palette.Transparent,
                Foreground = on ? Palette.Bar : Palette.Muted,
                PickCommand = new RelayCommand(pick),
            };
        }

        // ── saving and launching ──────────────────────────────────────────────────
        private void Touched()
        {
            Raise("DirtyVisibility");
            Raise("PlainName");
        }

        /// <summary>Everything the file carries, so a change anywhere lights the unsaved marker.</summary>
        private string Snapshot()
        {
            var text = new StringBuilder();
            text.Append(_name).Append('').Append(_isZombies).Append('').Append(Port).Append('')
                .Append(_shuffleOnLaunch).Append('').Append(_singleRoundDom).Append('')
                .Append(_botFill).Append('').Append(_botNames).Append('').Append(_botDifficulty).Append('')
                .Append(Cap).Append('').Append(MinPlayers).Append('').Append(StartDelay).Append('')
                .Append(_advertise).Append('').Append(string.Join("\n", ExtraLines)).Append('');
            foreach (var row in Rotation) text.Append(row.Entry.Gametype).Append(' ').Append(row.Entry.Map).Append(',');
            text.Append('');
            foreach (var mode in Modes()) text.Append(mode).Append('=').Append(_scores[mode]).Append(',');
            return text.ToString();
        }

        /// <summary>The editor's fields onto a preset. The one on screen is never the one written.</summary>
        private void Apply(ServerPreset target)
        {
            target.ServerName = _name;
            target.Mode = _isZombies ? "zombies" : "mp";
            target.Port = Port;
            target.ShuffleOnLaunch = _shuffleOnLaunch;
            target.SingleRoundDom = _singleRoundDom;
            target.BotFill = _botFill;
            target.BotNames = _botNames;
            target.BotDifficulty = _botDifficulty;
            target.MaxPlayers = Cap;
            target.MinPlayers = MinPlayers;
            target.StartDelay = StartDelay;
            target.Advertise = _advertise;
            target.ExtraLines.Clear();
            target.ExtraLines.AddRange(ExtraLines);
            foreach (var pair in _scores) target.ScoreLimits[pair.Key] = pair.Value;
            target.Rotation.Clear();
            foreach (var row in Rotation) target.Rotation.Add(row.Entry.Copy());
        }

        /// <summary>
        /// Writes the editor to its file. The edits go onto a candidate copy and the fleet only
        /// takes it once the write went through: a save that fails leaves the card, and anything
        /// started from it, on the configuration the file still holds.
        /// </summary>
        public bool Save()
        {
            var candidate = _preset.Copy();
            Apply(candidate);
            var shared = _fleet.PresetsOnPort(candidate.Port, candidate);
            if (!_fleet.SavePreset(candidate, IsNew)) return false;

            _preset = candidate;
            IsNew = false;
            FileChanged = false;
            _saved = Snapshot();
            Raise("DirtyVisibility"); Raise("OwnedPort"); Raise("StatusSub"); Raise("DeleteVisibility");
            _fleet.Toast(shared > 0
                ? "Saved " + candidate.FileName + ", but :" + candidate.Port + " is used by " + shared + " other " + (shared == 1 ? "preset" : "presets")
                : "Saved " + candidate.FileName);
            return true;
        }

        /// <summary>Save as writes a second preset. This one is left exactly as it was.</summary>
        private void SaveAs()
        {
            var name = _fleet.AskPresetName("Save preset as", _preset.FileName);
            if (name == null) return;
            var copy = _preset.Copy();
            Apply(copy);
            copy.FileName = name;
            copy.FilePath = _fleet.PathFor(name);
            _fleet.SaveCopy(copy);
        }

        /// <summary>A server is its preset, so Launch saves first, then starts it like the card does.</summary>
        private void Launch()
        {
            if (!Save()) return;
            // The same start the card's button runs, port check and all.
            _fleet.StartPreset(_preset);
        }

        private void Restart()
        {
            // The process is on the owned port; the save may be moving the preset to another one.
            var owned = OwnedPort;
            if (!Save()) return;
            _fleet.RestartPreset(_preset, owned);
        }

        /// <summary>Digits only, so a port or a score limit cannot be typed into nonsense.</summary>
        public static string Digits(string value, int length)
        {
            var kept = new StringBuilder(length);
            foreach (var c in value ?? "")
            {
                if (c < '0' || c > '9') continue;
                if (kept.Length == length) break;
                kept.Append(c);
            }
            return kept.ToString();
        }
    }
}
