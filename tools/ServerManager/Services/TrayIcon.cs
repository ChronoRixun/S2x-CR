using System;
using System.ComponentModel;
using System.Drawing;
using System.Windows.Forms;
using S2x.ServerManager.ViewModels;

namespace S2x.ServerManager.Services
{
    /// <summary>
    /// The tray icon: what the fleet is doing without the window open, and a way back to it.
    /// Closing the window hides it here rather than quitting, because a host who shut the
    /// manager would expect their servers to still be up — and they are. Nothing on this menu
    /// stops a server except Stop All, which says so.
    /// </summary>
    internal sealed class TrayIcon : IDisposable
    {
        private readonly System.Windows.Window _window;
        private readonly FleetViewModel _fleet;
        private readonly NotifyIcon _icon;
        private readonly ToolStripMenuItem _closeToTray;
        private bool _exiting;
        private bool _saidWhereItWent;

        public TrayIcon(System.Windows.Window window, FleetViewModel fleet)
        {
            _window = window;
            _fleet = fleet;

            var menu = new ContextMenuStrip
            {
                RenderMode = ToolStripRenderMode.System,
                BackColor = Color.FromArgb(0x16, 0x19, 0x1C),
                ForeColor = Color.FromArgb(0xE6, 0xE8, 0xEA),
                ShowImageMargin = false,
            };
            menu.Opening += (s, e) => Build(menu);

            _closeToTray = new ToolStripMenuItem("Close to tray") { Checked = true, CheckOnClick = true };

            _icon = new NotifyIcon
            {
                Icon = Mark(),
                Text = Tooltip(),
                Visible = true,
                ContextMenuStrip = menu,
            };
            _icon.DoubleClick += (s, e) => Open();

            _fleet.PropertyChanged += FleetChanged;
            _window.Closing += Closing;
        }

        /// <summary>Exit really quits; the window's close button only hides, if it is set to.</summary>
        private void Closing(object sender, CancelEventArgs e)
        {
            if (_exiting || !_closeToTray.Checked) { _icon.Visible = false; return; }
            e.Cancel = true;
            _window.Hide();
            if (_saidWhereItWent) return;
            _saidWhereItWent = true;
            _icon.ShowBalloonTip(3000, "S2x Server Manager",
                "Still running here. Your servers are untouched.", ToolTipIcon.None);
        }

        private void FleetChanged(object sender, PropertyChangedEventArgs e)
        {
            _icon.Text = Tooltip();
        }

        private string Tooltip()
        {
            // NotifyIcon.Text stops at 63 characters, which this never reaches.
            return "S2x Server Manager: " + _fleet.FleetUp + " up / " + _fleet.FleetTotal;
        }

        private void Build(ContextMenuStrip menu)
        {
            menu.Items.Clear();
            foreach (var card in _fleet.Servers)
            {
                var server = card;
                menu.Items.Add(new ToolStripMenuItem(
                    ":" + server.Preset.Port + "   " + server.PlainName + "   " + server.StateCaps,
                    null, (s, e) => Open(server)));
            }
            if (_fleet.Servers.Count > 0) menu.Items.Add(new ToolStripSeparator());

            menu.Items.Add(Item("Start all", () => _fleet.StartAllCommand.Execute(null),
                                _fleet.StartAllCommand.CanExecute(null)));
            menu.Items.Add(Item("Stop all", () => _fleet.StopAllCommand.Execute(null),
                                _fleet.StopAllCommand.CanExecute(null)));
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add(Item("Show", () => Open(), true));
            menu.Items.Add(_closeToTray);
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add(Item("Exit  (servers keep running)", Exit, true));
        }

        private static ToolStripMenuItem Item(string text, Action run, bool enabled)
        {
            return new ToolStripMenuItem(text, null, (s, e) => run()) { Enabled = enabled };
        }

        private void Open(ServerCardViewModel card = null)
        {
            _window.Show();
            if (_window.WindowState == System.Windows.WindowState.Minimized)
                _window.WindowState = System.Windows.WindowState.Normal;
            _window.Activate();
            if (card != null) _fleet.ShowServer(card);
        }

        private void Exit()
        {
            _exiting = true;
            _icon.Visible = false;
            System.Windows.Application.Current.Shutdown();
        }

        /// <summary>The exe's own icon: the amber mark Assets\make-icon.ps1 drew, embedded by
        /// &lt;ApplicationIcon&gt; at build time rather than composed again here.</summary>
        private static Icon Mark()
        {
            return Icon.ExtractAssociatedIcon(Application.ExecutablePath);
        }

        public void Dispose()
        {
            _fleet.PropertyChanged -= FleetChanged;
            _window.Closing -= Closing;
            _icon.Visible = false;
            _icon.Dispose();
        }
    }
}
