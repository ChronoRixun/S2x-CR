using System;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using S2x.ServerManager.Services;
using S2x.ServerManager.ViewModels;

namespace S2x.ServerManager
{
    public partial class App : Application
    {
        private const int ShotWidth = 1160;
        private const int ShotHeight = 740;

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            var demo = Argument(e.Args, "--demo");
            var demoEditor = Argument(e.Args, "--demo-editor");
            var shot = Argument(e.Args, "--screenshot");
            var shotEditor = Argument(e.Args, "--screenshot-editor");
            var presetDir = Argument(e.Args, "--presets");

            // --demo-editor <state> <png>, --screenshot-editor <png> [preset]
            var target = shot ?? shotEditor ?? (demoEditor != null ? Argument(e.Args, "--demo-editor", 2) : null);
            var wantedPreset = Argument(e.Args, "--screenshot-editor", 2);

            FleetViewModel fleet;
            if (demoEditor != null)
            {
                fleet = FleetViewModel.DemoEditor(demoEditor);
            }
            else if (demo != null)
            {
                fleet = FleetViewModel.Demo(demo);
            }
            else
            {
                var gameDir = GameFolder.Find() ?? (target == null ? GameFolder.Ask() : null);
                if (gameDir == null)
                {
                    const string missing = "Could not find s2x.exe. Start this from the game folder, " +
                                           "or pick the folder that holds s2x.exe.";
                    // A --screenshot run has nobody to answer a dialog.
                    if (target != null) Console.Error.WriteLine(missing);
                    else MessageBox.Show(missing, "S2x Server Manager", MessageBoxButton.OK, MessageBoxImage.Warning);
                    Shutdown(1);
                    return;
                }
                fleet = new FleetViewModel(gameDir, presetDir);
            }

            var view = Argument(e.Args, "--view");
            if (view != null) fleet.ViewMode = view;

            var window = new MainWindow { DataContext = fleet };
            MainWindow = window;

            if (target != null)
            {
                Capture(window, fleet, target, shotEditor != null ? () => OpenEditor(fleet, wantedPreset) : (Action)null);
                return;
            }

            window.Show();
            Dispatcher.InvokeAsync(async () => { await fleet.PollAsync(); fleet.StartPolling(); },
                DispatcherPriority.Background);
        }

        /// <summary>--screenshot-editor: the named preset, or the first one the fleet found.</summary>
        private static void OpenEditor(FleetViewModel fleet, string name)
        {
            var card = fleet.Servers.FirstOrDefault(s =>
                           string.Equals(s.Preset.FileName, name, StringComparison.OrdinalIgnoreCase))
                       ?? fleet.Servers.FirstOrDefault();
            if (card == null) return;
            fleet.OpenEditor(card.Preset);
        }

        /// <summary>--screenshot: render the window off-screen with real data and exit.</summary>
        private void Capture(MainWindow window, FleetViewModel fleet, string path, Action after)
        {
            window.Width = ShotWidth;
            window.Height = ShotHeight;
            window.WindowStartupLocation = WindowStartupLocation.Manual;
            window.Left = -20000;
            window.Top = -20000;
            window.ShowInTaskbar = false;
            window.ShowActivated = false;
            window.Show();

            Dispatcher.InvokeAsync(async () =>
            {
                var code = 0;
                try
                {
                    await fleet.PollAsync();
                    if (after != null) after();
                    window.UpdateLayout();
                    await Dispatcher.Yield(DispatcherPriority.ContextIdle);
                    window.UpdateLayout();
                    Save(window, path);
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine(ex.ToString());
                    code = 2;
                }
                window.Close();
                Shutdown(code);
            }, DispatcherPriority.Loaded);
        }

        private static void Save(Window window, string path)
        {
            var bitmap = new RenderTargetBitmap(ShotWidth, ShotHeight, 96, 96, PixelFormats.Pbgra32);
            bitmap.Render(window);
            var png = new PngBitmapEncoder();
            png.Frames.Add(BitmapFrame.Create(bitmap));
            var folder = Path.GetDirectoryName(Path.GetFullPath(path));
            if (!string.IsNullOrEmpty(folder)) Directory.CreateDirectory(folder);
            using (var file = File.Create(path)) png.Save(file);
        }

        /// <summary>The token after a switch, or the one after that when <paramref name="offset"/> is 2.</summary>
        private static string Argument(string[] args, string name, int offset = 1)
        {
            var index = Array.FindIndex(args, a => string.Equals(a, name, StringComparison.OrdinalIgnoreCase));
            if (index < 0) return null;
            var at = index + offset;
            if (at >= args.Length) return offset > 1 ? null : "";
            return args[at].StartsWith("--", StringComparison.Ordinal) && offset > 1 ? null : args[at];
        }
    }
}
