using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;

namespace S2x.ServerManager.ViewModels
{
    internal abstract class Observable : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;

        protected void Raise([CallerMemberName] string name = null)
        {
            var handler = PropertyChanged;
            if (handler != null) handler(this, new PropertyChangedEventArgs(name));
        }

        protected bool Set<T>(ref T field, T value, [CallerMemberName] string name = null)
        {
            if (Equals(field, value)) return false;
            field = value;
            Raise(name);
            return true;
        }

        /// <summary>Everything on a card is derived, so a refresh just re-reads the lot.</summary>
        protected void RaiseAll()
        {
            Raise(string.Empty);
        }
    }

    internal sealed class RelayCommand : ICommand
    {
        private readonly Action _run;
        private readonly Func<bool> _can;

        public RelayCommand(Action run, Func<bool> can = null)
        {
            _run = run;
            _can = can;
        }

        public event EventHandler CanExecuteChanged;

        public bool CanExecute(object parameter) { return _can == null || _can(); }

        public void Execute(object parameter) { _run(); }

        public void Refresh()
        {
            var handler = CanExecuteChanged;
            if (handler != null) handler(this, EventArgs.Empty);
        }
    }
}
