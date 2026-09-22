using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media;
using System.Windows.Threading;
using S2x.ServerManager.Models;
using S2x.ServerManager.Services;
using S2x.ServerManager.Views;

namespace S2x.ServerManager.ViewModels
{
    internal sealed class StarterViewModel
    {
        public string Index { get; set; }
        public string Title { get; set; }
        public string Description { get; set; }
        public string Meta { get; set; }
        public RelayCommand AddCommand { get; set; }
    }

    /// <summary>The fleet home: one card per preset, refreshed from a poll round every 3 s.</summary>
    internal sealed class FleetViewModel : Observable
    {
        private const int PollSeconds = 3;
        private const int QueryTimeoutMs = 2000;

        private readonly ServerController _controller;
        private readonly PresetStore _store;
        private readonly Dictionary<int, ServerState> _states = new Dictionary<int, ServerState>();
        private readonly DispatcherTimer _timer;
        private readonly bool _demo;

        private string _presetSignature = "";
        private bool _polling;
        private string _viewMode = "cards";
        private string _toast = "";
        private DispatcherTimer _toastTimer;

        public FleetViewModel(string gameDir)
        {
            _controller = new ServerController(gameDir);
            _store = new PresetStore(gameDir);
            Servers = new ObservableCollection<ServerCardViewModel>();
            BuildCommands();
            LoadStarters();

            _timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(PollSeconds) };
            _timer.Tick += async (s, e) => await PollAsync();
        }

        /// <summary>The mockup's demo states, so the four cards can be checked without servers.</summary>
        private FleetViewModel(IEnumerable<ServerCardViewModel> cards, List<StarterViewModel> starters)
        {
            _demo = true;
            Servers = new ObservableCollection<ServerCardViewModel>(cards);
            Starters = starters;
            BuildCommands();
            Recount();
        }

        public ObservableCollection<ServerCardViewModel> Servers { get; private set; }
        public List<StarterViewModel> Starters { get; private set; }

        public RelayCommand StartAllCommand { get; private set; }
        public RelayCommand StopAllCommand { get; private set; }
        public RelayCommand NewServerCommand { get; private set; }
        public RelayCommand ShowCardsCommand { get; private set; }
        public RelayCommand ShowRosterCommand { get; private set; }

        public void StartPolling() { if (!_demo) _timer.Start(); }

        // ── view switch ───────────────────────────────────────────────────────────
        public string ViewMode
        {
            get { return _viewMode; }
            set
            {
                if (!Set(ref _viewMode, value)) return;
                Raise("CardsVisibility"); Raise("RosterVisibility");
                Raise("CardsButtonBackground"); Raise("CardsButtonForeground");
                Raise("RosterButtonBackground"); Raise("RosterButtonForeground");
            }
        }

        public Visibility EmptyVisibility { get { return Servers.Count == 0 ? Visibility.Visible : Visibility.Collapsed; } }
        public Visibility CardsVisibility { get { return Servers.Count > 0 && _viewMode == "cards" ? Visibility.Visible : Visibility.Collapsed; } }
        public Visibility RosterVisibility { get { return Servers.Count > 0 && _viewMode == "roster" ? Visibility.Visible : Visibility.Collapsed; } }
        public Brush CardsButtonBackground { get { return _viewMode == "cards" ? Palette.TagBg : Palette.Transparent; } }
        public Brush CardsButtonForeground { get { return _viewMode == "cards" ? Palette.Accent : Palette.Muted; } }
        public Brush RosterButtonBackground { get { return _viewMode == "roster" ? Palette.TagBg : Palette.Transparent; } }
        public Brush RosterButtonForeground { get { return _viewMode == "roster" ? Palette.Accent : Palette.Muted; } }

        private ServerCardViewModel _selected;

        public ServerCardViewModel Selected
        {
            get { return _selected; }
            set
            {
                if (_selected != null) _selected.IsSelected = false;
                Set(ref _selected, value);
                if (_selected != null) _selected.IsSelected = true;
                Raise("HasSelection");
            }
        }

        public Visibility HasSelection { get { return _selected != null ? Visibility.Visible : Visibility.Collapsed; } }

        // ── fleet strip ───────────────────────────────────────────────────────────
        public int FleetUp { get; private set; }
        public int FleetTotal { get; private set; }
        public int FleetHumans { get; private set; }
        public int FleetCap { get; private set; }
        public int FleetBots { get; private set; }
        public int FleetAttention { get; private set; }
        public string FleetAttentionNote { get; private set; }
        public Brush FleetUpBrush { get { return FleetUp > 0 ? Palette.Ok : Palette.Muted; } }
        public Brush FleetAttentionBrush { get; private set; }

        public string Clock { get { return DateTime.Now.ToString("HH:mm"); } }

        // ── toast ─────────────────────────────────────────────────────────────────
        public string ToastText { get { return _toast; } }
        public Visibility ToastVisibility { get { return string.IsNullOrEmpty(_toast) ? Visibility.Collapsed : Visibility.Visible; } }

        public void Toast(string message)
        {
            _toast = message;
            Raise("ToastText");
            Raise("ToastVisibility");
            if (_toastTimer == null)
            {
                _toastTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(2.2) };
                _toastTimer.Tick += (s, e) =>
                {
                    _toastTimer.Stop();
                    _toast = "";
                    Raise("ToastText");
                    Raise("ToastVisibility");
                };
            }
            _toastTimer.Stop();
            _toastTimer.Start();
        }

        // ── commands ──────────────────────────────────────────────────────────────
        private void BuildCommands()
        {
            StartAllCommand = new RelayCommand(StartAll, () => Servers.Any(s => s.CanStart));
            StopAllCommand = new RelayCommand(StopAll, () => Servers.Any(s => s.CanStop));
            NewServerCommand = new RelayCommand(() => { }, () => false);   // the editor arrives in slice 2
            ShowCardsCommand = new RelayCommand(() => ViewMode = "cards");
            ShowRosterCommand = new RelayCommand(() => ViewMode = "roster");
        }

        public async void Start(ServerCardViewModel card)
        {
            if (_demo) return;
            var preset = card.Preset;
            var failure = await Task.Run(() => _controller.Start(preset));
            if (failure != null) Toast(failure);
            else Toast("Starting " + preset.PlainName + " on :" + preset.Port);
            await PollAsync();
        }

        public async void Stop(ServerCardViewModel card)
        {
            if (_demo) return;
            var port = card.Preset.Port;
            var failure = await Task.Run(() => _controller.Stop(port));
            if (failure != null) Toast(failure);
            await PollAsync();
        }

        public async void Restart(ServerCardViewModel card)
        {
            if (_demo) return;
            var preset = card.Preset;
            Toast("Restarting " + preset.PlainName);
            var failure = await Task.Run(() => { var stop = _controller.Stop(preset.Port); System.Threading.Thread.Sleep(1500); return stop ?? _controller.Start(preset); });
            if (failure != null) Toast(failure);
            await PollAsync();
        }

        private async void StartAll()
        {
            if (_demo) return;
            var queue = Servers.Where(s => s.CanStart).Select(s => s.Preset).OrderBy(p => p.Port).ToList();
            if (queue.Count == 0) return;
            Toast("Starting " + queue.Count + " server" + (queue.Count == 1 ? "" : "s"));
            await _controller.StartAllAsync(queue, Toast);
            await PollAsync();
        }

        private async void StopAll()
        {
            if (_demo) return;
            var ports = Servers.Where(s => s.CanStop).Select(s => s.Preset.Port).Distinct().ToList();
            await Task.Run(() => { foreach (var port in ports) _controller.Stop(port); });
            await PollAsync();
        }

        // ── the poll round ────────────────────────────────────────────────────────
        public async Task PollAsync()
        {
            if (_demo || _polling) return;
            _polling = true;
            try
            {
                ReloadPresetsIfChanged();
                var ports = Servers.Select(s => s.Preset.Port).Distinct().OrderBy(p => p).ToList();
                if (ports.Count == 0) { Recount(); return; }

                var round = await Task.Run(() => new Round
                {
                    Processes = ProcessInspector.DedicatedServers(),
                    Replies = ServerQuery.Query("127.0.0.1", ports, QueryTimeoutMs),
                });

                foreach (var port in ports) Apply(port, round);
                foreach (var card in Servers) { card.State = _states[card.Preset.Port]; card.Refresh(); }
                Recount();
            }
            finally
            {
                _polling = false;
            }
        }

        private sealed class Round
        {
            public Dictionary<int, S2xProcess> Processes;
            public Dictionary<int, ServerInfo> Replies;
        }

        private void Apply(int port, Round round)
        {
            ServerState state;
            if (!_states.TryGetValue(port, out state)) _states[port] = state = new ServerState { Port = port };

            S2xProcess process;
            var alive = round.Processes.TryGetValue(port, out process);
            ServerInfo info;
            var answered = round.Replies.TryGetValue(port, out info);

            if (alive)
            {
                state.Pid = process.Pid;
                state.ProcessStart = process.Started;
                state.NoticedGone = null;
            }

            if (answered)
            {
                state.Misses = 0;
                state.LastReply = DateTime.Now;
                state.PingMs = info.RttMs;
                state.Cap = Math.Max(1, info.Int("sv_maxclients", Math.Max(1, state.Cap)));
                state.Bots = Math.Max(0, info.Int("bots", 0));
                state.Humans = Math.Max(0, info.Int("clients", 0) - state.Bots);
                state.SvRunning = info.Get("sv_running") == "1";
                // In the lobby between maps the reply carries the next map as party_*.
                state.MapKey = Pick(info, "mapname", "party_mapname");
                state.GametypeKey = Pick(info, "gametype", "party_gametype");
                state.Status = ServerStatus.Running;
            }
            else if (alive)
            {
                state.Misses++;
                var young = (DateTime.Now - process.Started).TotalSeconds < ServerState.StartupGraceSeconds;
                if (state.LastReply == null && young) state.Status = ServerStatus.Starting;
                else if (state.Misses >= ServerState.MissesBeforeStale) state.Status = ServerStatus.NotAnswering;
                else if (state.Status == ServerStatus.Stopped || state.Status == ServerStatus.Crashed) state.Status = ServerStatus.Starting;
            }
            else
            {
                // No process. A pid file still there means nothing stopped it: Stop deletes it.
                var savedPid = ServerController.ReadPid(GameFolder.PidPath(_controller.GameDir, port));
                state.Misses = 0;
                state.Humans = state.Bots = state.PingMs = 0;
                state.ProcessStart = null;
                if (savedPid > 0)
                {
                    // "down 2 min" only when this app watched it go; a pid file found at startup
                    // says nothing about when the process died.
                    if (state.Status == ServerStatus.Running || state.Status == ServerStatus.Starting
                        || state.Status == ServerStatus.NotAnswering) state.NoticedGone = DateTime.Now;
                    state.Pid = savedPid;
                    state.Status = ServerStatus.Crashed;
                }
                else
                {
                    state.Pid = 0;
                    state.MapKey = null;
                    state.GametypeKey = null;
                    state.Cap = 0;
                    state.Status = ServerStatus.Stopped;
                }
            }
        }

        private static string Pick(ServerInfo info, string first, string second)
        {
            var value = info.Get(first);
            if (!string.IsNullOrEmpty(value)) return value;
            value = info.Get(second);
            return string.IsNullOrEmpty(value) ? null : value;
        }

        // ── presets ───────────────────────────────────────────────────────────────
        private void ReloadPresetsIfChanged()
        {
            var signature = Signature();
            if (signature == _presetSignature) return;
            _presetSignature = signature;

            var presets = _store.Load();
            var wanted = new List<ServerCardViewModel>();
            foreach (var preset in presets)
            {
                ServerState state;
                if (!_states.TryGetValue(preset.Port, out state)) _states[preset.Port] = state = new ServerState { Port = preset.Port };
                var existing = Servers.FirstOrDefault(s => s.Preset.FilePath == preset.FilePath);
                wanted.Add(existing != null && Same(existing.Preset, preset)
                    ? existing
                    : new ServerCardViewModel(this, preset, state));
            }

            Servers.Clear();
            foreach (var card in wanted) Servers.Add(card);
            if (Selected == null || !Servers.Contains(Selected)) Selected = Servers.FirstOrDefault();
            LoadStarters();
            Raise("EmptyVisibility"); Raise("CardsVisibility"); Raise("RosterVisibility");
        }

        private static bool Same(ServerPreset a, ServerPreset b)
        {
            return a.Port == b.Port && a.ServerName == b.ServerName && a.Mode == b.Mode
                && a.BotFill == b.BotFill && a.Rotation.Count == b.Rotation.Count;
        }

        private string Signature()
        {
            try
            {
                var dir = _store.Directory;
                if (!Directory.Exists(dir)) return "";
                return string.Join("|", Directory.GetFiles(dir, "*.json")
                    .Where(f => !Path.GetFileName(f).StartsWith("_", StringComparison.Ordinal))
                    .OrderBy(f => f, StringComparer.OrdinalIgnoreCase)
                    .Select(f => f + ":" + File.GetLastWriteTimeUtc(f).Ticks));
            }
            catch { return _presetSignature; }
        }

        private void LoadStarters()
        {
            var bundled = PresetStore.Bundled();
            var starters = new List<StarterViewModel>();
            for (int i = 0; i < bundled.Count; i++)
            {
                var preset = bundled[i];
                starters.Add(new StarterViewModel
                {
                    Index = (i + 1).ToString("00"),
                    Title = preset.FileName,
                    Description = Describe(preset),
                    Meta = string.Format("{0} maps {1} {2} bots {1} :{3}",
                        preset.Rotation.Count, GameData.MiddleDot, preset.BotFill, preset.Port),
                    AddCommand = new RelayCommand(() => AddStarter(preset)),
                });
            }
            Starters = starters;
            Raise("Starters");
        }

        private static string Describe(ServerPreset preset)
        {
            var maps = preset.Rotation.Select(r => GameData.MapName(r.Map)).Distinct().Take(4).ToList();
            var modes = preset.Rotation.Select(r => GameData.GametypeName(r.Gametype)).Distinct().Take(4).ToList();
            var dlc = preset.Rotation.Count(r => GameData.MapPack(r.Map) != null);
            var text = string.Join(", ", maps) + (preset.Rotation.Select(r => r.Map).Distinct().Count() > maps.Count ? " and more" : "") +
                       ". " + string.Join(", ", modes) + ".";
            if (dlc > 0) text += " Needs the DLC packs.";
            return text;
        }

        private void AddStarter(ServerPreset preset)
        {
            try
            {
                _store.Install(preset);
                _presetSignature = "";
                Toast("Added " + preset.FileName);
                ReloadPresetsIfChanged();
                Recount();
            }
            catch (Exception ex)
            {
                Toast(ex.Message);
            }
        }

        // ── totals ────────────────────────────────────────────────────────────────
        private void Recount()
        {
            FleetTotal = Servers.Count;
            FleetUp = Servers.Count(s => s.State.IsLive);
            FleetHumans = Servers.Where(s => s.State.Status == ServerStatus.Running).Sum(s => s.State.Humans);
            FleetBots = Servers.Where(s => s.State.IsLive || s.State.Status == ServerStatus.NotAnswering).Sum(s => s.State.Bots);
            FleetCap = Servers.Where(s => s.State.IsLive || s.State.Status == ServerStatus.NotAnswering).Sum(s => Math.Max(s.State.Cap, s.Preset.PlayerCap));
            FleetAttention = Servers.Count(s => s.State.NeedsAttention);

            var worst = Servers.FirstOrDefault(s => s.State.Status == ServerStatus.Crashed)
                     ?? Servers.FirstOrDefault(s => s.State.Status == ServerStatus.NotAnswering);
            FleetAttentionBrush = worst == null ? Palette.EdgeHot
                : worst.State.Status == ServerStatus.Crashed ? Palette.Danger : Palette.Accent;
            FleetAttentionNote = worst == null ? "all quiet"
                : ":" + worst.Preset.Port + (worst.State.Status == ServerStatus.Crashed ? " process gone" : " not answering");

            RaiseAll();
            StartAllCommand.Refresh();
            StopAllCommand.Refresh();
        }

        // ── demo ──────────────────────────────────────────────────────────────────
        public static FleetViewModel Demo(string state)
        {
            var fleet = new FleetViewModel(new List<ServerCardViewModel>(), new List<StarterViewModel>());
            foreach (var pair in DemoData.Build(state)) fleet.Servers.Add(new ServerCardViewModel(fleet, pair.Key, pair.Value));
            fleet.Starters = DemoData.Starters(fleet);
            fleet.Selected = fleet.Servers.FirstOrDefault();
            fleet.Recount();
            return fleet;
        }
    }
}
