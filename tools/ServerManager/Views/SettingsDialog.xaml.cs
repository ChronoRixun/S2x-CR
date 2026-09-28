using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Effects;
using S2x.ServerManager.Services;

namespace S2x.ServerManager.Views
{
    /// <summary>
    /// Settings → Appearance: a tile per theme in that theme's own colours, the Textures &amp;
    /// glow switch and a live badge preview. A pick applies at once and is saved; a save that
    /// fails says so and leaves the visible theme as it was.
    /// </summary>
    public partial class SettingsDialog : Window
    {
        private bool _changing;
        private readonly List<ThemeTile> _tiles;

        public SettingsDialog()
        {
            InitializeComponent();
            _tiles = ThemeCatalog.All.Select(t => new ThemeTile(t.Mode)).ToList();
            tiles.ItemsSource = _tiles;
            message.Text = ThemeManager.LoadWarning ?? "";
            Sync();

            effects.Checked += (s, e) => SetEffects(true);
            effects.Unchecked += (s, e) => SetEffects(false);
            barTitle.MouseLeftButtonDown += (s, e) => { if (e.ButtonState == MouseButtonState.Pressed) DragMove(); };
            btnClose.Click += (s, e) => Close();
            // Done is IsCancel: it closes the dialog the way Esc does.
            ThemeManager.Changed += ThemeChanged;
            Closed += (s, e) => ThemeManager.Changed -= ThemeChanged;
        }

        /// <summary>The tiles, in the catalog's order (tests read them).</summary>
        public IList<ThemeTile> Tiles { get { return _tiles; } }

        /// <summary>Applies and saves a theme; false (with the reason on screen) when the save failed.</summary>
        public bool Pick(ThemeMode mode)
        {
            try
            {
                ThemeManager.Apply(mode);
                message.Text = "";
                return true;
            }
            catch (Exception ex)
            {
                message.Text = "Could not save the theme. Your previous theme is unchanged. " + ex.Message;
                return false;
            }
            finally { Sync(); }
        }

        /// <summary>Turns Textures &amp; glow on or off and saves it; false when the save failed.</summary>
        public bool SetEffects(bool enabled)
        {
            if (_changing) return true;
            try
            {
                ThemeManager.SetEffects(enabled);
                message.Text = "";
                return true;
            }
            catch (Exception ex)
            {
                message.Text = "Could not save the setting. Textures & glow is unchanged. " + ex.Message;
                return false;
            }
            finally { Sync(); }
        }

        private void PickTile(object sender, RoutedEventArgs e)
        {
            var tile = ((FrameworkElement)sender).DataContext as ThemeTile;
            if (tile != null) Pick(tile.Mode);
        }

        private void ThemeChanged(object sender, EventArgs e) { Sync(); }

        /// <summary>Puts the ring, the switch and its note back in step with what is showing.</summary>
        private void Sync()
        {
            _changing = true;
            effects.IsChecked = ThemeManager.EffectsEnabled;
            _changing = false;
            effectsNote.Text = ThemeCatalog.Get(ThemeManager.Current).HasEffects
                ? "Scanlines, grain and neon. Off = flat colors."
                : "High contrast is always drawn flat. The switch applies to the other themes.";
            foreach (var tile in _tiles) tile.Refresh();
        }

        public static void Show(Window owner)
        {
            var dialog = new SettingsDialog();
            if (owner != null) dialog.Owner = owner;
            dialog.ShowDialog();
        }
    }

    /// <summary>One theme's tile: its own bg, fill, accent, display font and effects.</summary>
    public sealed class ThemeTile : INotifyPropertyChanged
    {
        public ThemeTile(ThemeMode mode) { Mode = mode; }

        public ThemeMode Mode { get; private set; }
        private ThemeDefinition Definition { get { return ThemeCatalog.Get(Mode); } }
        private ThemeResources Look { get { return ThemeResources.For(Mode, ThemeManager.EffectsEnabled); } }

        public string Name { get { return Definition.Name; } }
        public string Tagline { get { return Definition.Tagline; } }
        public bool IsSelected { get { return ThemeManager.Current == Mode; } }
        public Visibility RingVisibility { get { return IsSelected ? Visibility.Visible : Visibility.Collapsed; } }

        public Brush Accent { get { return Css.Solid(Look.Colors["Accent"]); } }
        public Brush Line { get { return Css.Solid(Look.Colors["Line"]); } }
        public Brush Ok { get { return Css.Solid(Look.Colors["Ok"]); } }
        public Brush Bad { get { return Css.Solid(Look.Colors["Danger"]); } }
        public Brush Fill { get { return Look.Fill; } }
        public Effect Glow { get { return Look.Glow; } }
        public Effect TextGlow { get { return Look.TextGlow; } }
        public CornerRadius Radius { get { return new CornerRadius(Look.Radius); } }
        public double RadiusValue { get { return Look.Radius; } }
        public CornerRadius RingRadius { get { return new CornerRadius(Look.Radius > 0 ? Look.Radius + 3 : 0); } }
        public FontFamily DisplayFont { get { return Look.Display.Family; } }
        public FontWeight DisplayWeight { get { return Look.Display.Weight; } }
        public FontStretch DisplayStretch { get { return Look.Display.Stretch; } }
        public double NameSize { get { return Look.Sizes["SizeTile"]; } }

        public event PropertyChangedEventHandler PropertyChanged;

        public void Refresh()
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(string.Empty));
        }
    }
}
