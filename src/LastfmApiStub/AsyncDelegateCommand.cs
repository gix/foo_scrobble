namespace LastfmApiStub
{
    using System;
    using System.Threading.Tasks;
    using System.Windows.Input;

    public class AsyncDelegateCommand : ICommand
    {
        private static readonly Func<bool> CanAlwaysExecute = () => true;
        private readonly Func<Task> execute;
        private readonly Func<bool> canExecute;
        private bool isExecuting;

        public AsyncDelegateCommand(Func<Task> execute)
            : this(execute, CanAlwaysExecute)
        {
        }

        public AsyncDelegateCommand(
            Func<Task> execute, Func<bool> canExecute)
        {
            this.execute = execute;
            this.canExecute = canExecute;
        }

        private bool IsExecuting
        {
            get => isExecuting;
            set
            {
                if (isExecuting != value) {
                    isExecuting = value;
                    RaiseCanExecuteChanged();
                }
            }
        }

        /// <summary>
        ///   Occurs when changes occur that affect whether or not the command
        ///   should execute.
        /// </summary>
        public event EventHandler? CanExecuteChanged
        {
            add => CommandManager.RequerySuggested += value;
            remove => CommandManager.RequerySuggested -= value;
        }

        /// <summary>
        ///   Defines the method that determines whether the command can execute
        ///   in its current state.
        /// </summary>
        /// <param name="parameter">
        ///   Data used by the command. If the command does not require data to
        ///   be passed, this object can be set to <see langword="null"/>.
        /// </param>
        /// <returns>
        ///   <see langword="true"/> if this command can be executed; otherwise,
        ///   <see langword="false"/>.
        /// </returns>
        public bool CanExecute(object? parameter)
        {
            return !IsExecuting && canExecute();
        }

        /// <summary>
        ///   Defines the method to be called when the command is invoked.
        /// </summary>
        /// <param name="parameter">
        ///   Data used by the command. If the command does not require data to
        ///   be passed, this object can be set to <see langword="null"/>.
        /// </param>
        public async void Execute(object? parameter)
        {
            if (!CanExecute(parameter))
                return;

            IsExecuting = true;
            try {
                await execute();
            } finally {
                IsExecuting = false;
            }
        }

        public void RaiseCanExecuteChanged()
        {
            OnCanExecuteChanged(EventArgs.Empty);
        }

        private void OnCanExecuteChanged(EventArgs e)
        {
            CommandManager.InvalidateRequerySuggested();
        }
    }

    public class AsyncDelegateCommand<T> : ICommand
        where T : class
    {
        private static readonly Func<T?, bool> CanAlwaysExecute = _ => true;
        private readonly Func<T?, Task> execute;
        private readonly Func<T?, bool> canExecute;
        private bool isExecuting;

        public AsyncDelegateCommand(Func<T?, Task> execute)
            : this(execute, CanAlwaysExecute)
        {
        }

        public AsyncDelegateCommand(
            Func<T?, Task> execute, Func<T?, bool> canExecute)
        {
            this.execute = execute;
            this.canExecute = canExecute;
        }

        private bool IsExecuting
        {
            get => isExecuting;
            set
            {
                if (isExecuting != value) {
                    isExecuting = value;
                    RaiseCanExecuteChanged();
                }
            }
        }

        /// <summary>
        ///   Occurs when changes occur that affect whether or not the command
        ///   should execute.
        /// </summary>
        public event EventHandler? CanExecuteChanged
        {
            add => CommandManager.RequerySuggested += value;
            remove => CommandManager.RequerySuggested -= value;
        }

        /// <summary>
        ///   Defines the method that determines whether the command can execute
        ///   in its current state.
        /// </summary>
        /// <param name="parameter">
        ///   Data used by the command. If the command does not require data to
        ///   be passed, this object can be set to <see langword="null"/>.
        /// </param>
        /// <returns>
        ///   <see langword="true"/> if this command can be executed; otherwise,
        ///   <see langword="false"/>.
        /// </returns>
        public bool CanExecute(object? parameter)
        {
            return !IsExecuting && canExecute((T?)parameter);
        }

        /// <summary>
        ///   Defines the method to be called when the command is invoked.
        /// </summary>
        /// <param name="parameter">
        ///   Data used by the command. If the command does not require data to
        ///   be passed, this object can be set to <see langword="null"/>.
        /// </param>
        public async void Execute(object? parameter)
        {
            if (!CanExecute(parameter))
                return;

            IsExecuting = true;
            try {
                await execute((T?)parameter);
            } finally {
                IsExecuting = false;
            }
        }

        public void RaiseCanExecuteChanged()
        {
            OnCanExecuteChanged(EventArgs.Empty);
        }

        protected virtual void OnCanExecuteChanged(EventArgs e)
        {
            CommandManager.InvalidateRequerySuggested();
        }
    }
}
