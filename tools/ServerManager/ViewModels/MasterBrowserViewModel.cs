using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Data;
using S2x.ServerManager.Models;
using S2x.ServerManager.Services;

namespace S2x.ServerManager.ViewModels
{
    internal sealed class BrowserRow
    {
        public MasterServerRow Source { get; private set; }
        public BrowserRow(MasterServerRow row) { Source = row; }
        public string Name { get { return string.IsNullOrWhiteSpace(Source.Name) ? Source.Address + ":" + Source.Port : Clean(Source.Name); } }
        public string Endpoint { get { return Source.Address + ":" + Source.Port; } }
        public string Map { get { return string.IsNullOrEmpty(Source.Map) ? "Unknown" : GameData.MapName(Source.Map); } }
        public string Mode { get { return string.IsNullOrEmpty(Source.GameType) ? "Unknown" : IsZombies ? "Zombies" : GameData.GametypeShort(Source.GameType); } }
        public string Humans { get { return Number(Source.Humans); } }
        public string Capacity { get { return Number(Source.MaxClients); } }
        public string Bots { get { return Number(Source.Bots); } }
        public string Ping { get { return Source.PingMs.HasValue ? Source.PingMs.Value + " ms" : "\u2014"; } }
        public string Status { get { return Source.Responded ? ((Source.Status ?? "").IndexOf("lobby", StringComparison.OrdinalIgnoreCase) >= 0 ? "Lobby" : "Responded") : "No reply"; } }
        public bool IsZombies { get { return string.Equals(Source.GameType, "zombies", StringComparison.OrdinalIgnoreCase); } }
        private static string Number(int? n) { return n.HasValue ? n.Value.ToString() : "?"; }
        private static string Clean(string value)
        {
            var result = System.Text.RegularExpressions.Regex.Replace(value, @"\^[0-9]", "");
            return new string(result.Where(c => !char.IsControl(c)).Take(160).ToArray());
        }
    }

    internal sealed class MasterBrowserViewModel : Observable, IDisposable
    {
        private readonly Func<string, CancellationToken, Task<MasterBrowserResult>> _refresh;
        private CancellationTokenSource _request;
        private bool _disposed, _busy;
        private string _search = "", _filter = "All", _status = "Refresh to query the S2x master list.";
        private DateTime? _updated;
        private int _listed;
        private BrowserRow _selected;
        public ObservableCollection<BrowserRow> Rows { get; private set; }
        public ICollectionView FilteredRows { get; private set; }
        public string[] Filters { get { return new[] { "All", "Zombies", "Multiplayer" }; } }
        public RelayCommand RefreshCommand { get; private set; }
        public RelayCommand CancelCommand { get; private set; }
        public RelayCommand CopyCommand { get; private set; }
        public MasterBrowserViewModel() : this(new MasterBrowserClient().RefreshAsync) { }
        internal MasterBrowserViewModel(Func<string, CancellationToken, Task<MasterBrowserResult>> refresh)
        {
            _refresh = refresh;
            Rows = new ObservableCollection<BrowserRow>();
            FilteredRows = CollectionViewSource.GetDefaultView(Rows);
            FilteredRows.Filter = Matches;
            RefreshCommand = new RelayCommand(async () => await RefreshAsync(), () => !_busy && !_disposed);
            CancelCommand = new RelayCommand(() => { if (_request != null) _request.Cancel(); }, () => _busy);
            CopyCommand = new RelayCommand(Copy, () => Selected != null);
        }
        public bool Busy { get { return _busy; } }
        public string Search { get { return _search; } set { if (Set(ref _search, value ?? "")) { Refilter(); } } }
        public string Filter { get { return _filter; } set { if (Set(ref _filter, value ?? "All")) { Refilter(); } } }
        public string Status { get { return _status; } private set { Set(ref _status, value); } }
        public BrowserRow Selected { get { return _selected; } set { if (Set(ref _selected, value)) CopyCommand.Refresh(); } }
        public string Updated { get { return _updated.HasValue ? "Last successful refresh: " + _updated.Value.ToLocalTime().ToString("HH:mm:ss") : "No successful refresh yet"; } }
        public string Summary
        {
            get
            {
                var shown = FilteredRows.Cast<object>().Count();
                return shown + " shown \u00b7 " + Rows.Count(r => r.Source.Responded) + " replied \u00b7 " + _listed + " listed";
            }
        }
        private void Refilter()
        {
            FilteredRows.Refresh();
            if (Selected != null && !Matches(Selected)) Selected = null;
            Raise("Summary");
        }
        private bool Matches(object value)
        {
            var row = value as BrowserRow;
            if (row == null) return false;
            if (_filter == "Zombies" && !row.IsZombies) return false;
            if (_filter == "Multiplayer" && (!row.Source.Responded || row.IsZombies || string.IsNullOrEmpty(row.Source.GameType))) return false;
            var needle = _search.Trim();
            return needle.Length == 0 || (row.Name + " " + row.Map + " " + row.Endpoint + " " + row.Mode).IndexOf(needle, StringComparison.OrdinalIgnoreCase) >= 0;
        }
        public async Task RefreshAsync()
        {
            if (_busy || _disposed) return;
            var cts = new CancellationTokenSource();
            _request = cts; _busy = true; NotifyBusy();
            Status = "Querying the master list and current server information...";
            try
            {
                var result = await _refresh("master.s2x.dev:20810", cts.Token);
                if (_disposed) return;
                if (cts.IsCancellationRequested) { Status = "Refresh cancelled. Previous results are unchanged."; return; }
                if (!string.IsNullOrEmpty(result.Error))
                {
                    Status = result.Error + (Rows.Count > 0 ? " Previous results are still displayed." : "");
                    return;
                }
                Selected = null;
                Rows.Clear();
                foreach (var row in result.Servers.OrderByDescending(r => r.Responded).ThenBy(r => r.Name, StringComparer.OrdinalIgnoreCase))
                    Rows.Add(new BrowserRow(row));
                _listed = result.ListedCount; _updated = result.CompletedUtc;
                FilteredRows.Refresh();
                Raise("Updated"); Raise("Summary");
                Status = string.IsNullOrEmpty(result.Warning)
                    ? (_listed == 0 ? "The master returned an empty server list." : "Maps and counts come from each server's latest reply. A missing reply does not prove a server is offline.")
                    : result.Warning;
            }
            catch (OperationCanceledException) { if (!_disposed) Status = "Refresh cancelled. Previous results are unchanged."; }
            catch (Exception ex) { if (!_disposed) Status = "Refresh failed: " + ex.Message + (Rows.Count > 0 ? " Previous results are still displayed." : ""); }
            finally
            {
                if (ReferenceEquals(_request, cts)) _request = null;
                cts.Dispose(); _busy = false;
                if (!_disposed) NotifyBusy();
            }
        }
        private void NotifyBusy() { Raise("Busy"); RefreshCommand.Refresh(); CancelCommand.Refresh(); }
        private void Copy()
        {
            try { if (Selected != null) { Clipboard.SetText("connect " + Selected.Endpoint); Status = "Connect command copied."; } }
            catch (Exception) { Status = "The clipboard is busy. Try copying again."; }
        }
        internal static MasterBrowserViewModel Demo()
        {
            var model = new MasterBrowserViewModel((host, token) => Task.FromResult(new MasterBrowserResult
            {
                ListedCount = 4, CompletedUtc = DateTime.UtcNow,
                Servers = new[] {
                    new MasterServerRow { Address="192.0.2.10", Port=27038, Name="Bodega squad", Map="mp_zombie_windmill_srv", GameType="zombies", Clients=4, Humans=1, Bots=3, MaxClients=4, PingMs=28, Responded=true },
                    new MasterServerRow { Address="192.0.2.11", Port=27040, Name="Olympus survival", Map="mp_zombie_dnk_srv", GameType="zombies", Clients=3, Humans=1, Bots=2, MaxClients=4, PingMs=42, Responded=true },
                    new MasterServerRow { Address="192.0.2.12", Port=27016, Name="Small map rotation", Map="mp_shipment_s2", GameType="war", Clients=12, Humans=3, Bots=9, MaxClients=18, PingMs=31, Responded=true },
                    new MasterServerRow { Address="192.0.2.13", Port=27017, Status="No reply" }
                }
            }));
            model.RefreshAsync().GetAwaiter().GetResult();
            model.Status = "Preview data only \u2014 no network requests.";
            return model;
        }

        public void Dispose() { _disposed = true; if (_request != null) _request.Cancel(); }
    }
}
