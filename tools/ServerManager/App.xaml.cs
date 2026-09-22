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
            var shot = Argument(e.Args, "--screenshot");

            FleetViewModel fleet;
            if (demo != null)
            {
                fleet = FleetViewModel.Demo(demo);
            }
            else
            {
                var gameDir = GameFolder.Find() ?? (shot == null ? GameFolder.Ask() : null);
                if (gameDir == null)
                {
                    const string missing = "Could not find s2x.exe. Start this from the game folder, " +
                                           "or pick the folder that holds s2x.exe.";
                    // A --screenshot run has nobody to answer a dialog.
                    if (shot != null) Console.Error.WriteLine(missing);
                    else MessageBox.Show(missing, "S2x Server Manager", MessageBoxButton.OK, MessageBoxImage.Warning);
                    Shutdown(1);
                    return;
                }
                fleet = new FleetViewModel(gameDir);
            }

            var view = Argument(e.Args, "--view");
            if (view != null) fleet.ViewMode = view;

            var window = new MainWindow { DataContext = fleet };
            MainWindow = window;

            if (shot != null) { Capture(window, fleet, shot); return; }

            window.Show();
            Dispatcher.InvokeAsync(async () => { await fleet.PollAsync(); fleet.StartPolling(); },
                DispatcherPriority.Background);
        }

        /// <summary>--screenshot: render the fleet window off-screen with real data and exit.</summary>
        private void Capture(MainWindow window, FleetViewModel fleet, string path)
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

        private static string Argument(string[] args, string name)
        {
            var index = Array.FindIndex(args, a => string.Equals(a, name, StringComparison.OrdinalIgnoreCase));
            if (index < 0) return null;
            return index + 1 < args.Length ? args[index + 1] : "";
        }
    }
}
