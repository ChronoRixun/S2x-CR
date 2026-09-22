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

        // One editor per preset, so the roster's pane and the full screen are the same edit and
        // unsaved work survives walking back to the fleet.
        private readonly Dictionary<string, EditorViewModel> _editors =
            new Dictionary<string, EditorViewModel>(StringComparer.OrdinalIgnoreCase);

        private string _presetSignature = "";
        private bool _polling;
        private string _viewMode = "cards";
        private string _screen = "fleet";
        private string _toast = "";
        private DispatcherTimer _toastTimer;
        private EditorViewModel _editor;

        public FleetViewModel(string gameDir, string presetDir = null)
        {
            GameDir = gameDir;
            _controller = new ServerController(gameDir);
            _store = new PresetStore(gameDir, presetDir);
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
            GameDir = @"D:\Steam\steamapps\common\Call of Duty WWII";
            Servers = new ObservableCollection<ServerCardViewModel>(cards);
            Starters = starters;
            BuildCommands();
            Recount();
        }

        public string GameDir { get; private set; }
        public bool IsDemo { get { return _demo; } }
        public string PresetDir { get { return _store == null ? "(demo)" : _store.Directory; } }

        public ObservableCollection<ServerCardViewModel> Servers { get; private set; }
        public List<StarterViewModel> Starters { get; private set; }

        public RelayCommand StartAllCommand { get; private set; }
        public RelayCommand StopAllCommand { get; private set; }
        public RelayCommand NewServerCommand { get; private set; }
        public RelayCommand ShowCardsCommand { get; private set; }
        public RelayCommand ShowRosterCommand { get; private set; }
        public RelayCommand SaveEditorCommand { get; private set; }
        public RelayCommand BackCommand { get; private set; }

        public void StartPolling() { if (!_demo) _timer.Start(); }

        // ── screens ───────────────────────────────────────────────────────────────
        public string Screen
        {
            get { return _screen; }
            set
            {
                if (!Set(ref _screen, value)) return;
                Raise("FleetVisibility"); Raise("EditorVisibility"); Raise("BackVisibility"); Raise("Breadcrumb");
            }
        }

        public EditorViewModel Editor
        {
            get { return _editor; }
            private set { Set(ref _editor, value); Raise("Breadcrumb"); }
        }

        public Visibility FleetVisibility { get { return _screen == "editor" ? Visibility.Collapsed : Visibility.Visible; } }
        public Visibility EditorVisibility { get { return _screen == "editor" ? Visibility.Visible : Visibility.Collapsed; } }
        public Visibility BackVisibility { get { return _screen == "editor" ? Visibility.Visible : Visibility.Collapsed; } }

        public string Breadcrumb
        {
            get
            {
                if (_screen != "editor" || _editor == null) return "SERVER MANAGER";
                return "SERVER MANAGER  ›  EDITOR  ›  " + _editor.PlainName.ToUpperInvariant();
            }
        }

        /// <summary>
        /// The one editor for this preset file, whether the roster pane or EDIT asked for it.
        /// A preset is its path: the same file read twice is still one server.
        /// </summary>
        public EditorViewModel EditorFor(ServerPreset preset, bool isNew = false)
        {
            if (preset == null) return null;
            var key = PresetStore.Key(preset.FilePath);
            EditorViewModel editor;
            if (!_editors.TryGetValue(key, out editor))
                _editors[key] = editor = new EditorViewModel(this, preset, isNew);
            return editor;
        }

        public void OpenEditor(ServerPreset preset, bool isNew = false)
        {
            Editor = EditorFor(preset, isNew);
            Screen = "editor";
        }

        /// <summary>
        /// Back to the fleet. A new server has no card to come back to, so it is saved or
        /// dropped here rather than left somewhere with no way in.
        /// </summary>
        public void ShowFleet()
        {
            if (_screen == "editor" && _editor != null && _editor.IsNew)
            {
                var answer = MessageBox.Show(
                    "This server has not been saved. Save it before going back?",
                    "New server", MessageBoxButton.YesNoCancel, MessageBoxImage.Question);
                if (answer == MessageBoxResult.Cancel) return;
                if (answer == MessageBoxResult.Yes && !_editor.Save()) return;
                if (answer == MessageBoxResult.No) Forget(_editor);
            }
            Screen = "fleet";
        }

        private void Forget(EditorViewModel editor)
        {
            var key = _editors.FirstOrDefault(pair => pair.Value == editor).Key;
            if (key != null) _editors.Remove(key);
            Editor = null;
        }

        /// <summary>The roster's right pane edits whatever row is selected.</summary>
        public EditorViewModel RosterEditor
        {
            get { return Selected == null ? null : EditorFor(Selected.Preset); }
        }

        /// <summary>The editor the host is actually looking at, if any.</summary>
        public EditorViewModel VisibleEditor
        {
            get
            {
                if (_screen == "editor") return _editor;
                return _viewMode == "roster" ? RosterEditor : null;
            }
        }

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
                Raise("RosterEditor");
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
            NewServerCommand = new RelayCommand(NewServer);
            ShowCardsCommand = new RelayCommand(() => ViewMode = "cards");
            ShowRosterCommand = new RelayCommand(() => ViewMode = "roster");
            // Ctrl+S saves whichever editor is on screen: the full one, or the roster's pane.
            SaveEditorCommand = new RelayCommand(() => { var editor = VisibleEditor; if (editor != null) editor.Save(); });
            BackCommand = new RelayCommand(ShowFleet);
        }

        public void Start(ServerCardViewModel card) { StartPreset(card.Preset); }

        public async void StartPreset(ServerPreset preset)
        {
            if (_demo) return;
            if (!PortIsOurs(preset)) return;
            // The worker gets a copy: an edit landing while it works would otherwise change the
            // port it has already checked, or the rotation it is halfway through writing.
            var snapshot = preset.Copy();
            var failure = await Task.Run(() => _controller.Start(snapshot));
            if (failure != null) Toast(failure);
            else Toast("Starting " + snapshot.PlainName + " on :" + snapshot.Port);
            await PollAsync();
        }

        /// <summary>
        /// A port belongs to one server: the second one to ask for the socket never gets it, so
        /// say so instead of leaving a card that starts and dies. Every launch path asks.
        /// </summary>
        private bool PortIsOurs(ServerPreset preset)
        {
            if (PresetsOnPort(preset.Port, preset) == 0) return true;
            Toast("Two servers cannot share :" + preset.Port + ". Give one of them a free port first.");
            return false;
        }

        public void Stop(ServerCardViewModel card) { StopPort(card.Preset.Port); }

        public async void StopPort(int port)
        {
            if (_demo) return;
            var failure = await Task.Run(() => _controller.Stop(port));
            if (failure != null) Toast(failure);
            await PollAsync();
        }

        public void Restart(ServerCardViewModel card) { RestartPreset(card.Preset, card.Preset.Port); }

        /// <summary>Stops the port this server owns, then starts the same snapshot on it.</summary>
        public async void RestartPreset(ServerPreset preset, int ownedPort)
        {
            if (_demo) return;
            // Ask before stopping: a shared port would stop the other preset's server and put
            // this configuration up in its place.
            if (!PortIsOurs(preset)) return;
            var snapshot = preset.Copy();
            Toast("Restarting " + snapshot.PlainName);
            var failure = await Task.Run(() =>
            {
                var stop = _controller.Stop(ownedPort);
                System.Threading.Thread.Sleep(1500);
                return stop ?? _controller.Start(snapshot);
            });
            if (failure != null) Toast(failure);
            await PollAsync();
        }

        // ── presets the editor writes ─────────────────────────────────────────────
        /// <summary>How many other presets claim this port. A preset is its file, not its object.</summary>
        public int PresetsOnPort(int port, ServerPreset except)
        {
            var mine = except == null ? null : except.FilePath;
            return Servers.Count(s => s.Preset.Port == port && !PresetStore.SamePath(s.Preset.FilePath, mine));
        }

        public string PathFor(string name) { return _store.PathFor(name); }

        public ServerState StateFor(int port)
        {
            ServerState state;
            if (_states.TryGetValue(port, out state)) return state;
            return new ServerState { Port = port };
        }

        /// <summary>
        /// Writes a preset file and puts what was written in front of the host. The candidate is
        /// written first and adopted afterwards, so a write that fails leaves the fleet on the
        /// configuration its file still holds. The signature moves with our own write.
        /// </summary>
        public bool SavePreset(ServerPreset candidate, bool isNew)
        {
            if (_demo) { Toast("Demo mode: nothing was written"); return false; }
            try
            {
                _store.Save(candidate, isNew);
                _presetSignature = Signature();

                var card = Servers.FirstOrDefault(s => PresetStore.SamePath(s.Preset.FilePath, candidate.FilePath));
                if (card != null) card.Adopt(candidate);
                else
                {
                    ServerState state;
                    if (!_states.TryGetValue(candidate.Port, out state)) _states[candidate.Port] = state = new ServerState { Port = candidate.Port };
                    Servers.Add(new ServerCardViewModel(this, candidate, state));
                    Raise("EmptyVisibility"); Raise("CardsVisibility"); Raise("RosterVisibility");
                }
                foreach (var each in Servers) each.Refresh();
                Recount();
                return true;
            }
            catch (Exception ex)
            {
                Toast("Could not write the preset: " + ex.Message);
                return false;
            }
        }

        /// <summary>Save as: a second preset file, a second server, opened in place of this one.</summary>
        public void SaveCopy(ServerPreset copy)
        {
            // The original keeps its port, so the copy needs one of its own.
            copy.Port = PresetStore.NextFreePort(Servers.Select(s => s.Preset.Port));
            if (!SavePreset(copy, true)) return;
            Toast("Saved " + copy.FileName + " on :" + copy.Port);
            OpenEditor(copy);
        }

        /// <summary>
        /// The Save as prompt. Refuses what the launcher's preset box could not show, and refuses
        /// a name that is taken: writing over another preset would lose it without asking.
        /// </summary>
        public string AskPresetName(string title, string suggestion)
        {
            while (true)
            {
                var entered = Views.PromptDialog.Ask(title, "Preset name", suggestion);
                if (entered == null) return null;
                var problem = _store.NameProblem(entered, true);
                if (problem == null) return entered.Trim();
                Toast(problem);
                suggestion = entered;
            }
        }

        private void NewServer()
        {
            if (_demo) { Toast("Demo mode: nothing was written"); return; }
            var choices = new List<Views.StarterChoice>
            {
                new Views.StarterChoice
                {
                    Title = "Blank server",
                    Summary = "No maps yet, everything else at the launcher's defaults.",
                    Meta = "0 maps",
                    Blank = true,
                },
            };
            foreach (var bundled in PresetStore.Bundled())
                choices.Add(new Views.StarterChoice
                {
                    Title = bundled.FileName,
                    Summary = Describe(bundled),
                    Meta = bundled.Rotation.Count + " maps",
                    Source = bundled,
                });

            var picked = Views.NewServerDialog.Ask(choices);
            if (picked == null) return;

            ServerPreset preset;
            if (picked.Blank)
            {
                preset = new ServerPreset { ServerName = "^7New server", FileName = "New server" };
                foreach (var pair in GameData.DefaultScoreLimits) preset.ScoreLimits[pair.Key] = pair.Value;
                preset.FileName = _store.Exists(preset.FileName) ? FreeName(preset.FileName) : preset.FileName;
            }
            else
            {
                // A bundled starter is seeded under its own name the once, the way the launcher's
                // Update-PresetList seeds it, and never over a preset the host already has: the
                // second server off the same starter becomes its own file.
                var starter = (ServerPreset)picked.Source;
                preset = starter.Copy();   // keys the starter carries and this app does not know come too
                preset.FileName = _store.Exists(starter.FileName) ? FreeName(starter.FileName) : starter.FileName;
            }
            preset.FilePath = _store.PathFor(preset.FileName);
            preset.Port = PresetStore.NextFreePort(Servers.Select(s => s.Preset.Port));
            OpenEditor(preset, true);
        }

        private string FreeName(string name)
        {
            for (int i = 2; i < 100; i++)
                if (!_store.Exists(name + " " + i)) return name + " " + i;
            return name + " copy";
        }

        private async void StartAll()
        {
            if (_demo) return;
            // A port belongs to one server here too: a port two presets claim is left alone
            // rather than started as whichever of them came first.
            var shared = Servers.Where(s => s.CanStart).Select(s => s.Preset.Port)
                .GroupBy(port => port).Where(g => Servers.Count(s => s.Preset.Port == g.Key) > 1)
                .Select(g => g.Key).ToList();
            var queue = Servers.Where(s => s.CanStart && !shared.Contains(s.Preset.Port))
                .Select(s => s.Preset.Copy()).OrderBy(p => p.Port).ToList();
            foreach (var port in shared.Distinct())
                Toast("Two servers cannot share :" + port + ". Give one of them a free port first.");
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
                    Scan = ProcessInspector.DedicatedServers(),
                    Replies = ServerQuery.Query("127.0.0.1", ports, QueryTimeoutMs),
                });

                foreach (var port in ports) Apply(port, round);
                foreach (var card in Servers) { card.State = _states[card.Preset.Port]; card.Refresh(); }
                // An open editor keeps its state line and its NOW marker up to date too.
                foreach (var editor in _editors.Values) editor.RefreshState();
                Recount();
            }
            catch (Exception ex)
            {
                // Drop the round and try again in three seconds. A poll that throws must not
                // take the app down with it.
                System.Diagnostics.Debug.WriteLine("poll round failed: " + ex);
                Toast("Could not read the servers: " + ex.Message);
            }
            finally
            {
                _polling = false;
            }
        }

        private sealed class Round
        {
            public ServerScan Scan;
            public Dictionary<int, ServerInfo> Replies;
        }

        private void Apply(int port, Round round)
        {
            ServerState state;
            if (!_states.TryGetValue(port, out state)) _states[port] = state = new ServerState { Port = port };

            S2xProcess process;
            var alive = round.Scan.Servers.TryGetValue(port, out process);
            ServerInfo info;
            var answered = round.Replies.TryGetValue(port, out info);

            if (alive)
            {
                // A different pid on the port is a different server: forget what the old one
                // answered, or a restart would burn its 90 s of starting grace in nine seconds.
                if (state.Pid != process.Pid)
                {
                    state.LastReply = null;
                    state.Misses = 0;
                }
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
            else if (!round.Scan.Complete && state.Status != ServerStatus.Stopped)
            {
                // The process list could not be read this round, so "no process" means nothing:
                // leave the state where it was rather than call a live server gone.
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
                // Whatever the file says now is what this preset is: the card takes the new
                // reading rather than deciding for itself which fields count as a change.
                var existing = Servers.FirstOrDefault(s => PresetStore.SamePath(s.Preset.FilePath, preset.FilePath));
                if (existing != null) { existing.Adopt(preset); existing.State = state; wanted.Add(existing); }
                else wanted.Add(new ServerCardViewModel(this, preset, state));
                Reconcile(preset);
            }

            Servers.Clear();
            foreach (var card in wanted) Servers.Add(card);
            if (Selected == null || !Servers.Contains(Selected)) Selected = Servers.FirstOrDefault();
            LoadStarters();
            Raise("EmptyVisibility"); Raise("CardsVisibility"); Raise("RosterVisibility"); Raise("RosterEditor");
        }

        /// <summary>
        /// The file changed under an open editor. An editor with nothing unsaved is rebuilt on
        /// what the file says; one with a draft in it keeps the draft and says the file moved.
        /// </summary>
        private void Reconcile(ServerPreset preset)
        {
            var key = PresetStore.Key(preset.FilePath);
            EditorViewModel editor;
            if (!_editors.TryGetValue(key, out editor)) return;
            if (editor.Preset == preset) return;

            if (editor.IsDirty || editor.IsNew) { editor.FileChanged = true; return; }
            var replacement = new EditorViewModel(this, preset, false);
            _editors[key] = replacement;
            if (_editor == editor) Editor = replacement;
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
            // A port belongs to one server. Say on the card when two presets claim the same one.
            foreach (var card in Servers)
            {
                card.SharedPorts = PresetsOnPort(card.Preset.Port, card.Preset);
                card.Refresh();
            }

            FleetTotal = Servers.Count;
            FleetUp = Servers.Count(s => s.State.IsLive);
            FleetHumans = Servers.Where(s => s.State.Status == ServerStatus.Running).Sum(s => s.State.Humans);
            FleetBots = Servers.Where(s => s.State.IsLive || s.State.Status == ServerStatus.NotAnswering).Sum(s => s.State.Bots);
            FleetCap = Servers.Where(s => s.State.IsLive || s.State.Status == ServerStatus.NotAnswering).Sum(s => Math.Max(s.State.Cap, s.Preset.MaxPlayers));
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
            foreach (var pair in DemoData.Build(state))
            {
                fleet._states[pair.Key.Port] = pair.Value;
                fleet.Servers.Add(new ServerCardViewModel(fleet, pair.Key, pair.Value));
            }
            fleet.Starters = DemoData.Starters(fleet);
            fleet.Selected = fleet.Servers.FirstOrDefault();
            fleet.Recount();
            return fleet;
        }

        /// <summary>--demo-editor: one server, opened in the editor, no game folder read.</summary>
        public static FleetViewModel DemoEditor(string state)
        {
            var fleet = new FleetViewModel(new List<ServerCardViewModel>(), new List<StarterViewModel>());
            var pair = DemoData.Editor(state);
            fleet._states[pair.Key.Port] = pair.Value;
            fleet.Servers.Add(new ServerCardViewModel(fleet, pair.Key, pair.Value));
            fleet.Starters = DemoData.Starters(fleet);
            fleet.Selected = fleet.Servers[0];
            fleet.Recount();
            fleet.OpenEditor(pair.Key);
            // The multiplayer state is the one with something not written yet.
            if (string.Equals(state, "mp", StringComparison.OrdinalIgnoreCase)) fleet.Editor.ShuffleOnLaunch = true;
            return fleet;
        }
    }
}
