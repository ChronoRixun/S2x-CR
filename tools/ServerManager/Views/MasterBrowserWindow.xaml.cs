using System;
using System.Windows;
using S2x.ServerManager.ViewModels;

namespace S2x.ServerManager.Views
{
    public partial class MasterBrowserWindow : Window
    {
        private readonly MasterBrowserViewModel _model;
        public MasterBrowserWindow() : this(new MasterBrowserViewModel(), true) { }
        internal MasterBrowserWindow(MasterBrowserViewModel model, bool refreshOnLoad)
        {
            _model = model;
            InitializeComponent(); DataContext = model;
            if (refreshOnLoad) Loaded += async (s, e) => await _model.RefreshAsync();
            Closed += (s, e) => _model.Dispose();
        }
    }
}
