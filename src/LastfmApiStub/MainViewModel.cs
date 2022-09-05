namespace LastfmApiStub
{
    using System;
    using System.Collections.ObjectModel;
    using System.Threading.Tasks;
    using System.Windows;
    using System.Windows.Input;
    using System.Windows.Threading;
    using LastfmApiStub.Controllers;
    using Microsoft.Extensions.Logging;

    public enum ErrorResponseKind
    {
        Api_InvalidService = LastfmStatus.InvalidService,
        Api_InvalidMethod = LastfmStatus.InvalidMethod,
        Api_AuthenticationFailed = LastfmStatus.AuthenticationFailed,
        Api_InvalidFormat = LastfmStatus.InvalidFormat,
        Api_InvalidParameters = LastfmStatus.InvalidParameters,
        Api_InvalidResourceSpecified = LastfmStatus.InvalidResourceSpecified,
        Api_OperationFailed = LastfmStatus.OperationFailed,
        Api_InvalidSessionKey = LastfmStatus.InvalidSessionKey,
        Api_InvalidAPIKey = LastfmStatus.InvalidAPIKey,
        Api_ServiceOffline = LastfmStatus.ServiceOffline,
        Api_InvalidMethodSignature = LastfmStatus.InvalidMethodSignature,
        Api_TokenNotAuthorized = LastfmStatus.TokenNotAuthorized,
        Api_ServiceTemporarilyUnavailable = LastfmStatus.ServiceTemporarilyUnavailable,
        Api_SuspendedAPIKey = LastfmStatus.SuspendedAPIKey,
        Api_RateLimitExceeded = LastfmStatus.RateLimitExceeded,
        InvalidContentType = -1,
        EncodingError = -2,
    }

    public class MainViewModel : ObservableModel, ILastfmRequestLogger, ILogItemSink
    {
        private readonly Dispatcher dispatcher;
        private LastmApiRequest? selectedRequest;
        private bool useSlowResponse;
        private int slowResponseTime = 2000;
        private bool useErrorResponse;
        private ErrorResponseKind selectedErrorResponseKind;

        public MainViewModel(ApiStubSettings apiStubSettings)
        {
            ExitCommand = new AsyncDelegateCommand(OnExit);
            dispatcher = Dispatcher.CurrentDispatcher;

            foreach (var kind in Enum.GetValues<ErrorResponseKind>())
                ErrorResponseKinds.Add(kind);
        }

        private Task OnExit()
        {
            Application.Current.Shutdown();
            return Task.CompletedTask;
        }

        public ICommand ExitCommand { get; }

        public ObservableCollection<LastmApiRequest> Requests { get; } = new();

        public ObservableCollection<LogItem> LogItems { get; } = new();

        public LastmApiRequest? SelectedRequest
        {
            get => selectedRequest;
            set => SetProperty(ref selectedRequest, value);
        }

        public bool UseSlowResponse
        {
            get => useSlowResponse;
            set => SetProperty(ref useSlowResponse, value);
        }

        public int SlowResponseTime
        {
            get => slowResponseTime;
            set => SetProperty(ref slowResponseTime, value);
        }

        public bool UseErrorResponse
        {
            get => useErrorResponse;
            set => SetProperty(ref useErrorResponse, value);
        }

        public ObservableCollection<ErrorResponseKind> ErrorResponseKinds { get; } = new();

        public ErrorResponseKind SelectedErrorResponseKind
        {
            get => selectedErrorResponseKind;
            set => SetProperty(ref selectedErrorResponseKind, value);
        }

        int? ILastfmRequestLogger.SlowResponseTime => UseSlowResponse ? SlowResponseTime : null;
        ErrorResponseKind? ILastfmRequestLogger.ErrorResponseKind => UseErrorResponse ? SelectedErrorResponseKind : null;

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
