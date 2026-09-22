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
            var shotConsole = Argument(e.Args, "--screenshot-console");
            var demoRoster = Argument(e.Args, "--demo-roster");
            var presetDir = Argument(e.Args, "--presets");

            // --demo-editor <state> <png>, --screenshot-editor <png> [preset]
            var target = shot ?? shotEditor ?? shotConsole ?? demoRoster
                         ?? (demoEditor != null ? Argument(e.Args, "--demo-editor", 2) : null);
            var wantedPreset = Argument(e.Args, "--screenshot-editor", 2);

            // A render switch that cannot render must say so and stop, not fall through and
            // leave a window open on a machine nobody is watching.
            var problem = Usage(demoEditor, shotEditor, shotConsole, demoRoster, target);
            if (problem != null) { Stop(problem, 2); return; }

            FleetViewModel fleet;
            if (demoEditor != null)
            {
                fleet = FleetViewModel.DemoEditor(demoEditor);
            }
            else if (shotConsole != null)
            {
                // The console on a made-up server and a log written here: no game folder read,
                // and nothing written into one.
                fleet = FleetViewModel.DemoEditor("mp");
            }
            else if (demoRoster != null)
            {
                fleet = FleetViewModel.Demo("three-notanswering");
                fleet.ViewMode = "roster";
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
                    if (target == null) MessageBox.Show(missing, "S2x Server Manager", MessageBoxButton.OK, MessageBoxImage.Warning);
                    Stop(missing, 1);
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
                Func<string> after = null;
                if (shotEditor != null) after = () => OpenEditor(fleet, wantedPreset);
                else if (shotConsole != null) after = () => OpenDemoConsole(fleet);
                Capture(window, fleet, target, after);
                return;
            }

            window.Show();
            Dispatcher.InvokeAsync(async () => { await fleet.PollAsync(); fleet.StartPolling(); },
                DispatcherPriority.Background);
        }

        /// <summary>
        /// Gives up before there is a window to close. Shutdown() only takes effect once the
        /// dispatcher is running, and a switch that cannot be honoured must not reach that far.
        /// </summary>
        private static void Stop(string message, int code)
        {
            Console.Error.WriteLine(message);
            Environment.Exit(code);
        }

        /// <summary>What is wrong with the render switches, or null when they can be honoured.</summary>
        private static string Usage(string demoEditor, string shotEditor, string shotConsole,
                                    string demoRoster, string target)
        {
            if (demoEditor != null)
            {
                if (Array.IndexOf(new[] { "mp", "zombies", "empty" }, demoEditor.ToLowerInvariant()) < 0)
                    return "--demo-editor takes mp, zombies or empty.";
                if (string.IsNullOrEmpty(target)) return "--demo-editor <state> <png>: no png was given.";
            }
            if (shotEditor != null && string.IsNullOrEmpty(shotEditor))
                return "--screenshot-editor <png> [preset]: no png was given.";
            if (shotConsole != null && string.IsNullOrEmpty(shotConsole))
                return "--screenshot-console <png>: no png was given.";
            if (demoRoster != null && string.IsNullOrEmpty(demoRoster))
                return "--demo-roster <png>: no png was given.";
            return null;
        }

        /// <summary>
        /// --screenshot-console: the drawer on a log this switch writes itself, because there is
        /// no server running and the game folder's own logs are not ours to touch.
        /// </summary>
        private static string OpenDemoConsole(FleetViewModel fleet)
        {
            var card = fleet.Servers.FirstOrDefault();
            if (card == null) return "The demo fleet has no server to open a console on.";
            var path = Path.Combine(Path.GetTempPath(), "s2x-server-" + card.Preset.Port + ".log");
            File.WriteAllLines(path, Services.DemoData.ConsoleLog(card.Preset), new System.Text.UTF8Encoding(false));
            fleet.Console.ShowFile(card, path);
            return null;
        }

        /// <summary>
        /// --screenshot-editor: the preset that was asked for, or the only sensible one when no
        /// name was given. A name that is not there is an error, not the first preset instead.
        /// </summary>
        private static string OpenEditor(FleetViewModel fleet, string name)
        {
            if (!string.IsNullOrEmpty(name))
            {
                var wanted = fleet.Servers.FirstOrDefault(s =>
                    string.Equals(s.Preset.FileName, name, StringComparison.OrdinalIgnoreCase));
                if (wanted == null) return "No preset called " + name + " in " + fleet.PresetDir + ".";
                fleet.OpenEditor(wanted.Preset);
                return null;
            }

            var first = fleet.Servers.FirstOrDefault();
            if (first == null) return "No presets in " + fleet.PresetDir + ", so there is no editor to render.";
            fleet.OpenEditor(first.Preset);
            return null;
        }

        /// <summary>--screenshot: render the window off-screen with real data and exit.</summary>
        private void Capture(MainWindow window, FleetViewModel fleet, string path, Func<string> after)
        {
            // The capture closes its own window; the exit code has to survive that.
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
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
                    var failure = after == null ? null : after();
                    if (failure != null)
                    {
                        // Writing the fleet under the name of an editor shot would be a lie.
                        Console.Error.WriteLine(failure);
                        code = 3;
                    }
                    else
                    {
                        window.UpdateLayout();
                        await Dispatcher.Yield(DispatcherPriority.ContextIdle);
                        window.UpdateLayout();
                        Save(window, path);
                    }
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
