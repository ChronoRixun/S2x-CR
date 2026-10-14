using System;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using S2x.ServerManager.Services;

namespace S2x.ServerManager.Views
{
    public partial class AdminWindow : Window
    {
        private readonly int _port;
        private readonly ServerAdminClient _client;
        private bool _busy, _available, _notices;
        public AdminWindow(int port, string name, ServerAdminClient client = null)
        {
            InitializeComponent();
            _port = port; _client = client ?? new ServerAdminClient();
            heading.Text = (name ?? "Server") + " :" + port;
            refresh.Click += async (s, e) => await Refresh();
            players.SelectionChanged += (s, e) => UpdateButtons();
            message.TextChanged += (s, e) => UpdateButtons();
            announce.Click += async (s, e) => await Act("announce");
            warn.Click += async (s, e) => await Act("warn");
            kick.Click += async (s, e) => await Act("kick");
            Loaded += async (s, e) => await Refresh();
            UpdateButtons();
        }
        private void UpdateButtons()
        {
            var selected = players.SelectedItem as AdminPlayer;
            var target = selected != null && !selected.isBot && !selected.isHost;
            var text = !string.IsNullOrWhiteSpace(message.Text) && Encoding.UTF8.GetByteCount(message.Text) <= 160;
            refresh.IsEnabled = !_busy;
            announce.IsEnabled = !_busy && _available && _notices && text;
            warn.IsEnabled = !_busy && _available && _notices && target && text;
            kick.IsEnabled = !_busy && _available && target && text;
        }
        private async Task Refresh()
        {
            _busy = true; UpdateButtons(); status.Text = "Checking this Manager's server ownership and current players...";
            var reply = await _client.RequestAsync(_port, "players");
            _available = reply.ok; _notices = reply.noticeReady;
            players.ItemsSource = reply.ok ? reply.players : null;
            players.SelectedItem = null;
            status.Text = reply.ok ? "Player list refreshed. Select a human to warn or kick. " + (_notices ? "Notices are available." : "Notice script unavailable: announcements and warnings are disabled.") : reply.message;
            _busy = false; UpdateButtons();
        }
        private async Task Act(string operation)
        {
            var selected = players.SelectedItem as AdminPlayer;
            var target = operation == "announce" ? "everyone on " + heading.Text : selected?.name;
            if (target == null) return;
            var text = message.Text.Trim();
            var question = operation == "announce" ? "Send this announcement to " : operation == "warn" ? "Send this warning to " : "Kick ";
            if (MessageBox.Show(this, question + target + "?\n\n" + text,
                "Confirm server action", MessageBoxButton.YesNo, MessageBoxImage.Question, MessageBoxResult.No) != MessageBoxResult.Yes) return;
            _busy = true; UpdateButtons();
            var reply = await _client.RequestAsync(_port, operation, operation == "announce" ? null : selected.token, text);
            status.Text = (reply.ok ? "Server accepted the action. Client display is not confirmed. " : "Action was not confirmed. ") + reply.message;
            if (!string.IsNullOrEmpty(reply.AuditWarning)) status.Text += " " + reply.AuditWarning;
            // Explicit refresh obtains new connection tokens; never retry a warning or kick.
            if (operation == "kick" || !reply.ok) { players.ItemsSource = null; _available = false; }
            _busy = false; UpdateButtons();
        }
    }
}
