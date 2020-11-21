namespace LastfmApiStub
{
    using System.Collections.ObjectModel;
    using System.Threading.Tasks;
    using System.Windows;
    using System.Windows.Input;
    using System.Windows.Threading;
    using Microsoft.Extensions.Logging;

    public class MainViewModel : ObservableModel, ILastfmRequestLogger, ILogItemSink
    {
        private readonly Dispatcher dispatcher;
        private LastmApiRequest? selectedRequest;

        public MainViewModel(ApiStubSettings apiStubSettings)
        {
            ExitCommand = new AsyncDelegateCommand(OnExit);
            dispatcher = Dispatcher.CurrentDispatcher;
        }

        private Task OnExit()
        {
            Application.Current.Shutdown();
            return Task.CompletedTask;
        }

        public ICommand ExitCommand { get; }

        public ObservableCollection<LastmApiRequest> Requests { get; } =
            new ObservableCollection<LastmApiRequest>();

        public ObservableCollection<LogItem> LogItems { get; } =
            new ObservableCollection<LogItem>();

        public LastmApiRequest? SelectedRequest
        {
            get => selectedRequest;
            set => SetProperty(ref selectedRequest, value);
        }

        void ILastfmRequestLogger.Log(LastmApiRequest request)
        {
            if (!dispatcher.CheckAccess()) {
                dispatcher.InvokeAsync(() => ((ILastfmRequestLogger)this).Log(request));
                return;
            }

            Requests.Add(request);
        }

        void ILogItemSink.Log(LogItem item)
        {
            if (!dispatcher.CheckAccess()) {
                dispatcher.InvokeAsync(() => ((ILogItemSink)this).Log(item));
                return;
            }

            LogItems.Add(item);
        }
    }

    public class LogItem
    {
        public LogItem(LogLevel logLevel, in EventId eventId, string message)
        {
            LogLevel = logLevel;
            EventId = eventId;
            Message = message;
        }

        public LogLevel LogLevel { get; }
        public EventId EventId { get; }
        public string Message { get; }
    }
}
