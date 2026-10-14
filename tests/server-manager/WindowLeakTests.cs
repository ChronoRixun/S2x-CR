using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Media;
using System.Windows.Threading;

// Closed windows must be collectable, and the status badge's breath must run only while the
// badge is on screen. Hosts a plain WPF Application with the Manager's Theme.xaml, never the
// Manager's App (its startup would run on the real game folder and settings), on demo data:
// no game folder, no presets, no servers, no network. The settings file is a temp path that
// is never written.
//
// A closed main window used to stay reachable for good: a row bound without
// INotifyPropertyChanged (a segment, a swatch) was held by WPF's PropertyDescriptor table,
// its command held the editor, and the editor's CaretSet held the view. Every window a run
// opened made the ones after it slower to lay out, and each kept its breathing badges ticking.
// Either half of that chain is enough to hold a window in the Manager, where the fleet lives
// as long as the process, so the test checks both: every bound object says when it changes,
// and a window is collected while its view models are still alive.
class WindowLeakTests
{
    const BindingFlags Any = BindingFlags.Static | BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;
    static Assembly asm;
    static string temp;
    /// <summary>The view models, kept alive past their windows the way the Manager keeps its fleet.</summary>
    static readonly List<object> Kept = new List<object>();
    /// <summary>Bound objects WPF could only watch through PropertyDescriptor, by window.</summary>
    static readonly SortedSet<string> Unwatchable = new SortedSet<string>();

    static void Require(bool ok, string why) { if (!ok) throw new Exception("FAIL " + why); Console.WriteLine("PASS " + why); }
    static void Pump(DispatcherPriority priority) { Dispatcher.CurrentDispatcher.Invoke(new Action(() => { }), priority); }
    static Type T(string name) { return asm.GetType("S2x.ServerManager." + name, true); }

    static object Keep(object model) { Kept.Add(model); return model; }
    static object Fleet(string kind, string state) { return Keep(T("ViewModels.FleetViewModel").GetMethod(kind, Any).Invoke(null, new object[] { state })); }

    static Window Home(object fleet)
    {
        var window = (Window)Activator.CreateInstance(T("MainWindow"));
        window.DataContext = fleet;
        window.Width = 1160; window.Height = 740;
        return window;
    }

    static readonly string[] Kinds = { "fleet", "roster", "editor", "settings", "master browser", "admin" };

    static Window Make(string kind)
    {
        switch (kind)
        {
            case "fleet": return Home(Fleet("Demo", "three-notanswering"));
            case "roster":
                var fleet = Fleet("Demo", "three-notanswering");
                fleet.GetType().GetProperty("ViewMode").SetValue(fleet, "roster", null);
                return Home(fleet);
            case "editor": return Home(Fleet("DemoEditor", "mp"));
            case "settings": return (Window)Activator.CreateInstance(T("Views.SettingsDialog"));
            case "master browser":
                // The demo rows, and no refresh: nothing goes to the network.
                var model = Keep(T("ViewModels.MasterBrowserViewModel").GetMethod("Demo", Any).Invoke(null, null));
                return (Window)T("Views.MasterBrowserWindow").GetConstructor(Any, null, new[] { model.GetType(), typeof(bool) }, null)
                    .Invoke(new object[] { model, false });
            case "admin":
                // An empty ownership store in the temp folder: the refresh is refused before
                // anything is sent, because this run owns no server.
                var store = Activator.CreateInstance(T("Services.ManagedServerOwnershipStore"), temp);
                var client = Keep(Activator.CreateInstance(T("Services.ServerAdminClient"), store));
                return (Window)Activator.CreateInstance(T("Views.AdminWindow"), 55443, "Leak test", client);
        }
        throw new ArgumentException(kind);
    }

    static void Show(Window window)
    {
        window.WindowStartupLocation = WindowStartupLocation.Manual; window.Left = -20000; window.Top = -20000;
        window.ShowInTaskbar = false; window.ShowActivated = false;
        window.Show(); window.UpdateLayout();
        Pump(DispatcherPriority.ContextIdle);
        window.UpdateLayout();
    }

    static IEnumerable<DependencyObject> Tree(DependencyObject node)
    {
        yield return node;
        for (int i = 0; i < VisualTreeHelper.GetChildrenCount(node); i++)
            foreach (var child in Tree(VisualTreeHelper.GetChild(node, i))) yield return child;
    }

    /// <summary>
    /// Every data context in the window raises PropertyChanged or is a DependencyObject. WPF
    /// watches anything else through PropertyDescriptor.ValueChanged, whose table holds the
    /// object (and whatever it holds) for as long as a binding to it is alive.
    /// </summary>
    static void CheckBindings(string kind, Window window)
    {
        foreach (var context in Tree(window).OfType<FrameworkElement>().Select(e => e.DataContext).Where(c => c != null).Distinct())
        {
            var type = context.GetType();
            if (context is INotifyPropertyChanged || context is DependencyObject || type.IsValueType || context is string) continue;
            if (type.FullName == "MS.Internal.NamedObject") continue;   // {DisconnectedItem}, a recycled container
            Unwatchable.Add(kind + ": " + type.FullName);
        }
    }

    // Its own frame, so nothing on this method's stack still holds the window afterwards.
    [MethodImpl(MethodImplOptions.NoInlining)]
    static WeakReference OpenAndClose(string kind)
    {
        var window = Make(kind);
        Show(window);
        CheckBindings(kind, window);
        window.Close();
        return new WeakReference(window);
    }

    static int StillReachable(List<KeyValuePair<string, WeakReference>> closed, out string which)
    {
        // WPF lets go of a closed window in stages (input, bindings, the collection views it
        // keeps for a while), so settle a few times before counting.
        for (int round = 0; round < 5; round++)
        {
            Pump(DispatcherPriority.SystemIdle);
            GC.Collect(); GC.WaitForPendingFinalizers(); GC.Collect();
        }
        var alive = closed.Where(c => c.Value.IsAlive).Select(c => c.Key).ToList();
        which = string.Join(", ", alive.GroupBy(k => k).Select(g => g.Count() + " " + g.Key));
        return alive.Count;
    }

    /// <summary>Every breathing badge in the window, and whether its dot is animated.</summary>
    static List<KeyValuePair<FrameworkElement, bool>> Breaths(Window window)
    {
        var badge = T("Views.StatusBadge");
        return Tree(window).OfType<FrameworkElement>()
            .Where(e => badge.IsInstanceOfType(e) && (bool)badge.GetProperty("Pulse").GetValue(e, null))
            .Select(e => new KeyValuePair<FrameworkElement, bool>(e,
                ((UIElement)((System.Windows.Controls.Control)e).Template.FindName("dot", e)).HasAnimatedProperties))
            .ToList();
    }

    static void CheckBreath()
    {
        var window = Make("fleet");
        Show(window);
        var shown = Breaths(window);
        Require(shown.Any(b => b.Key.IsVisible), "the demo fleet shows a running server's breathing badge");
        Require(shown.All(b => b.Value == b.Key.IsVisible),
            "a running badge breathes while it is on screen and not in a collapsed pane (" +
            shown.Count(b => b.Value) + " breathing, " + shown.Count(b => !b.Key.IsVisible) + " hidden)");
        window.Hide(); Pump(DispatcherPriority.ContextIdle);
        Require(Breaths(window).All(b => !b.Value), "no badge breathes while the window is closed to the tray");
        window.Show(); window.UpdateLayout(); Pump(DispatcherPriority.ContextIdle);
        Require(Breaths(window).Where(b => b.Key.IsVisible).All(b => b.Value), "the breath starts again when the window comes back");
        window.Close(); Pump(DispatcherPriority.ContextIdle);
        Require(Breaths(window).All(b => !b.Value), "no badge breathes once its window is closed");
    }

    [STAThread]
    static int Main(string[] args)
    {
        temp = Path.Combine(Path.GetTempPath(), "s2x-window-leak-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        var never = Path.Combine(temp, "never-written.json");
        Application app = null;
        try
        {
            asm = Assembly.LoadFrom(Path.GetFullPath(args[0]));
            app = new Application { ShutdownMode = ShutdownMode.OnExplicitShutdown };
            app.Resources.MergedDictionaries.Add((ResourceDictionary)Application.LoadComponent(
                new Uri("/" + asm.GetName().Name + ";component/theme.xaml", UriKind.Relative)));
            var themes = T("Services.ThemeManager");
            themes.GetMethod("Initialize").Invoke(null, new object[] { never });
            Pump(DispatcherPriority.ContextIdle);

            CheckBreath();

            // Every window, three times, each in another theme, so the recoloured brushes and
            // the replaced resources are live while windows come and go.
            var modes = Enum.GetValues(T("Services.ThemeMode"));
            var apply = themes.GetMethod("Apply", new[] { T("Services.ThemeMode"), typeof(bool), typeof(bool) });
            var closed = new List<KeyValuePair<string, WeakReference>>();
            int turn = 0;
            for (int round = 0; round < 3; round++)
                foreach (var kind in Kinds)
                {
                    apply.Invoke(null, new object[] { modes.GetValue(turn++ % modes.Length), true, false });
                    closed.Add(new KeyValuePair<string, WeakReference>(kind, OpenAndClose(kind)));
                }
            Require(Unwatchable.Count == 0, "every object the windows bind to raises PropertyChanged or is a DependencyObject" +
                (Unwatchable.Count == 0 ? "" : "; not: " + string.Join(", ", Unwatchable)));
            string which;
            var reachable = StillReachable(closed, out which);
            Require(reachable == 0, closed.Count + " closed windows (" + string.Join(", ", Kinds) + ") are all collected while their " +
                Kept.Count + " view models live on" + (reachable == 0 ? "" : "; still reachable: " + which));
            GC.KeepAlive(Kept);
            Require(!File.Exists(never), "the test never wrote settings");
            return 0;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine(e is TargetInvocationException && e.InnerException != null ? e.InnerException : e);
            return 1;
        }
        finally
        {
            if (app != null) app.Shutdown();
            try { Directory.Delete(temp, true); } catch { }
        }
    }
}
