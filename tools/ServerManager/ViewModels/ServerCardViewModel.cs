using System;
using System.Collections.Generic;
using System.Windows;
using System.Windows.Media;
using S2x.ServerManager.Models;
using S2x.ServerManager.Views;

namespace S2x.ServerManager.ViewModels
{
    internal sealed class SlotTick
    {
        public Brush Fill { get; set; }
        public Brush Stroke { get; set; }
    }

    internal sealed class PingBar
    {
        public double Height { get; set; }
        public Brush Fill { get; set; }
    }

    /// <summary>One card: a preset, its port and whatever the last poll round found there.</summary>
    internal sealed class ServerCardViewModel : Observable
    {
        private static readonly Brush TickFree = Palette.Edge;
        private static readonly Brush TickBot = Palette.EdgeHot;
        private static readonly double[] BarHeights = { 4, 7, 10, 12 };

        private readonly FleetViewModel _fleet;
        private bool _isSelected;

        public ServerCardViewModel(FleetViewModel fleet, ServerPreset preset, ServerState state)
        {
            _fleet = fleet;
            Preset = preset;
            State = state;
            StartCommand = new RelayCommand(() => _fleet.Start(this), () => CanStart);
            StopCommand = new RelayCommand(() => _fleet.Stop(this), () => CanStop);
            RestartCommand = new RelayCommand(() => _fleet.Restart(this));
            CopyCommand = new RelayCommand(Copy);
            SelectCommand = new RelayCommand(() => _fleet.Selected = this);
            EditCommand = new RelayCommand(() => _fleet.OpenEditor(Preset));
        }

        /// <summary>How many other presets claim this card's port; set by the fleet's count.</summary>
        public int SharedPorts { get; set; }

        public string SharedNote
        {
            get { return "shares :" + Preset.Port + " with " + SharedPorts + (SharedPorts == 1 ? " preset" : " presets"); }
        }

        public Visibility SharedVisibility { get { return Show(SharedPorts > 0); } }

        public ServerPreset Preset { get; private set; }
        public ServerState State { get; set; }

        /// <summary>The card follows its file: a fresh reading, or the one just written.</summary>
        public void Adopt(ServerPreset preset)
        {
            Preset = preset;
            Refresh();
        }

        public RelayCommand StartCommand { get; private set; }
        public RelayCommand StopCommand { get; private set; }
        public RelayCommand RestartCommand { get; private set; }
        public RelayCommand CopyCommand { get; private set; }
        public RelayCommand SelectCommand { get; private set; }
        public RelayCommand EditCommand { get; private set; }

        public void Refresh()
        {
            RaiseAll();
            StartCommand.Refresh();
            StopCommand.Refresh();
        }

        // ── identity ──────────────────────────────────────────────────────────────
        public string PortText { get { return Preset.Port.ToString(); } }
        public string ModeBadge { get { return Preset.IsZombies ? "ZM" : "MP"; } }
        public string Name { get { return Preset.ServerName; } }
        public string PlainName { get { return Preset.PlainName; } }
        public string Address { get { return "127.0.0.1:" + Preset.Port; } }

        // ── state ─────────────────────────────────────────────────────────────────
        public string StateCaps
        {
            get
            {
                switch (State.Status)
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
                switch (State.Status)
                {
                    case ServerStatus.Running: return Palette.Ok;
                    case ServerStatus.Starting:
                    case ServerStatus.NotAnswering: return Palette.Accent;
                    case ServerStatus.Crashed: return Palette.Danger;
                    default: return Palette.Muted;
                }
            }
        }

        public Brush DotBrush { get { return State.Status == ServerStatus.Stopped ? Palette.Off : StateBrush; } }
        public bool IsStarting { get { return State.Status == ServerStatus.Starting; } }

        public Brush CardBackground
        {
            get
            {
                if (State.Status == ServerStatus.NotAnswering) return Palette.TagBg;
                if (State.Status == ServerStatus.Crashed) return Palette.DangerBg;
                return Palette.Bar;
            }
        }

        public Brush CardEdge
        {
            get
            {
                if (State.Status == ServerStatus.NotAnswering) return Palette.TagEdge;
                if (State.Status == ServerStatus.Crashed) return Palette.DangerEdge;
                return Palette.Line;
            }
        }

        public Brush CornerBrush
        {
            get
            {
                switch (State.Status)
                {
                    case ServerStatus.NotAnswering: return Palette.Accent;
                    case ServerStatus.Crashed: return Palette.Danger;
                    case ServerStatus.Running: return Palette.EdgeHot;
                    default: return Palette.Edge;
                }
            }
        }

        public string UptimeLine
        {
            get
            {
                if (State.IsLive || State.Status == ServerStatus.NotAnswering)
                    return "up " + ServerState.FormatSpan(State.Uptime);
                if (State.Status == ServerStatus.Crashed && State.NoticedGone.HasValue)
                    return "down " + ServerState.FormatSpan(DateTime.Now - State.NoticedGone.Value);
                return "";
            }
        }

        // ── plates ────────────────────────────────────────────────────────────────
        public Visibility LivePlate { get { return Show(State.IsLive); } }
        public Visibility HealthPlate { get { return Show(State.NeedsAttention); } }
        public Visibility StoppedPlate { get { return Show(State.Status == ServerStatus.Stopped); } }
        public Visibility NextRow { get { return Show(!State.NeedsAttention && Preset.Rotation.Count > 0); } }
        public string CapText { get { return State.Humans + "/" + Math.Max(State.Cap, Preset.MaxPlayers); } }
        public string UptimeText { get { return ServerState.FormatSpan(State.Uptime); } }

        public string MapName
        {
            get
            {
                var entry = CurrentEntry;
                if (State.MapKey != null) return GameData.MapName(State.MapKey);
                return entry != null ? GameData.MapName(entry.Map) : "—";
            }
        }

        public string GametypeCaps
        {
            get
            {
                if (Preset.IsZombies) return "ZOMBIES";
                var key = State.GametypeKey ?? (CurrentEntry != null ? CurrentEntry.Gametype : null);
                return key == null ? "" : GameData.GametypeName(key).ToUpperInvariant();
            }
        }

        public string NowLine
        {
            get
            {
                if (State.Status == ServerStatus.Stopped) return "—";
                if (State.Status == ServerStatus.Crashed) return "exited";
                return MapName + " " + GameData.MiddleDot + " " +
                       (Preset.IsZombies ? "Zombies" : GameData.GametypeName(State.GametypeKey ?? ""));
            }
        }

        public List<SlotTick> Slots
        {
            get
            {
                var cap = Math.Max(1, State.Cap > 0 ? State.Cap : Preset.MaxPlayers);
                var ticks = new List<SlotTick>(cap);
                for (int i = 0; i < cap; i++)
                {
                    if (i < State.Humans) ticks.Add(new SlotTick { Fill = Palette.Accent, Stroke = Palette.Accent });
                    else if (i < State.Humans + State.Bots) ticks.Add(new SlotTick { Fill = TickBot, Stroke = TickBot });
                    else ticks.Add(new SlotTick { Fill = Palette.Transparent, Stroke = TickFree });
                }
                return ticks;
            }
        }

        public string HumansText { get { return State.Humans.ToString(); } }
        public string BotsText { get { return State.Bots.ToString(); } }
        public string FreeText { get { return State.Free.ToString(); } }
        public string PingText { get { return State.PingMs.ToString(); } }

        public List<PingBar> PingBars
        {
            get
            {
                var ping = State.PingMs;
                int level = ping <= 0 ? 0 : ping < 35 ? 4 : ping < 70 ? 3 : ping < 120 ? 2 : 1;
                var bars = new List<PingBar>(4);
                for (int i = 0; i < BarHeights.Length; i++)
                    bars.Add(new PingBar
                    {
                        Height = BarHeights[i],
                        Fill = i < level ? (level >= 3 ? Palette.Ok : Palette.Accent) : Palette.Edge,
                    });
                return bars;
            }
        }

        public string NextLabel { get { return State.IsLive ? "NEXT" : "FIRST"; } }

        public List<string> NextThree
        {
            get
            {
                var rotation = Preset.Rotation;
                var next = new List<string>();
                if (rotation.Count == 0) return next;

                var start = State.IsLive ? CurrentIndex : -1;
                for (int i = 1; i <= 3 && i <= rotation.Count; i++)
                    next.Add(rotation[((start + i) % rotation.Count + rotation.Count) % rotation.Count].Label(Preset.IsZombies));
                return next;
            }
        }

        public string RotationSummary
        {
            get
            {
                return string.Format("{0} in rotation {1} {2} bots {1} cap {3}",
                    Preset.Rotation.Count, GameData.MiddleDot, Preset.BotFill, Preset.MaxPlayers);
            }
        }

        public string HealthCaps
        {
            get { return State.Status == ServerStatus.NotAnswering ? "PROCESS ALIVE, NOT ANSWERING" : "PROCESS IS GONE"; }
        }

        public string HealthLine
        {
            get
            {
                if (State.Status == ServerStatus.NotAnswering)
                {
                    var since = State.LastReply.HasValue
                        ? ServerState.FormatSpan(DateTime.Now - State.LastReply.Value)
                        : "a while";
                    var seen = State.MapKey != null
                        ? " Last seen on " + MapName + " " + GameData.MiddleDot + " " +
                          (Preset.IsZombies ? "Zombies" : GameData.GametypeName(State.GametypeKey ?? "")) + "."
                        : "";
                    return "No status reply for " + since + "." + seen + " A restart usually clears it.";
                }
                if (State.Status == ServerStatus.Crashed)
                    return "s2x.exe on :" + Preset.Port + " is not running, and nothing here stopped it: " +
                           "its pid file is still in the game folder. Start brings it back on the same port.";
                return "";
            }
        }

        public string HealthMono
        {
            get
            {
                if (State.Status == ServerStatus.NotAnswering)
                    return "PID " + State.Pid + " alive " + GameData.MiddleDot + " last reply " +
                           (State.LastReply.HasValue ? State.LastReply.Value.ToString("HH:mm:ss") : "never");
                if (State.Status == ServerStatus.Crashed)
                    return "PID " + State.Pid + " gone " + GameData.MiddleDot + @" s2x\server-" + Preset.Port + ".pid";
                return "";
            }
        }

        public string StatusSub
        {
            get
            {
                if (State.IsLive || State.Status == ServerStatus.NotAnswering)
                    return "PID " + State.Pid + " " + GameData.MiddleDot + " " + Address;
                if (State.Status == ServerStatus.Crashed)
                    return "PID " + State.Pid + " exited " + GameData.MiddleDot + " " + Address;
                return "no process";
            }
        }

        // ── actions ───────────────────────────────────────────────────────────────
        public bool CanStart { get { return State.Status == ServerStatus.Stopped || State.Status == ServerStatus.Crashed; } }
        public bool CanStop { get { return !CanStart; } }
        public Visibility StartButton { get { return Show(CanStart); } }
        public Visibility StopButton { get { return Show(CanStop); } }
        public Visibility RestartButton { get { return Show(State.Status == ServerStatus.NotAnswering); } }

        // ── roster ────────────────────────────────────────────────────────────────
        public bool IsSelected
        {
            get { return _isSelected; }
            set { if (Set(ref _isSelected, value)) { Raise("RowBackground"); Raise("RowMarker"); Raise("RowForeground"); } }
        }

        public Brush RowBackground { get { return _isSelected ? Palette.Panel : Palette.Transparent; } }
        public Brush RowMarker { get { return _isSelected ? Palette.Accent : Palette.Transparent; } }
        public Brush RowForeground { get { return _isSelected ? Palette.Ink : Palette.Label; } }

        private void Copy()
        {
            try
            {
                Clipboard.SetText("connect " + Address);
                _fleet.Toast("Copied  connect " + Address);
            }
            catch (Exception ex)
            {
                _fleet.Toast(ex.Message);
            }
        }

        private RotationEntry CurrentEntry
        {
            get
            {
                var index = CurrentIndex;
                if (index >= 0 && index < Preset.Rotation.Count) return Preset.Rotation[index];
                return Preset.Rotation.Count > 0 ? Preset.Rotation[0] : null;
            }
        }

        /// <summary>Where the reply's map sits in the preset's rotation, or -1 when it is not in it.</summary>
        private int CurrentIndex
        {
            get
            {
                if (State.MapKey == null) return -1;
                for (int i = 0; i < Preset.Rotation.Count; i++)
                    if (Preset.Rotation[i].Map == State.MapKey && Preset.Rotation[i].Gametype == State.GametypeKey) return i;
                for (int i = 0; i < Preset.Rotation.Count; i++)
                    if (Preset.Rotation[i].Map == State.MapKey) return i;
                return -1;
            }
        }

        private static Visibility Show(bool on) { return on ? Visibility.Visible : Visibility.Collapsed; }
    }
}
