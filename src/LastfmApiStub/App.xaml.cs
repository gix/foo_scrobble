namespace LastfmApiStub
{
    using System;
    using System.Collections.Generic;
    using System.ComponentModel;
    using System.Linq;
    using System.Text.Json;
    using System.Threading;
    using System.Windows;
    using Microsoft.AspNetCore.Builder;
    using Microsoft.AspNetCore.Hosting;
    using Microsoft.AspNetCore.Mvc;
    using Microsoft.Extensions.DependencyInjection;
    using Microsoft.Extensions.Hosting;
    using Microsoft.Extensions.Logging;
    using Microsoft.Extensions.Primitives;

    public partial class App
    {
        private readonly ApiStubSettings settings = new ApiStubSettings();
        private readonly Thread serverThread;
        private IHost? host;
        private MainViewModel? mainViewModel;

        public App()
        {
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            PresentationTheme.Aero.ThemeManager.Install();
            PresentationTheme.Aero.AeroTheme.SetAsCurrentTheme();

            serverThread = new Thread(ServerThread);
        }

        private void ServerThread()
        {
            host = CreateHostBuilder(Array.Empty<string>()).Build();
            host.Run();
        }

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            mainViewModel = new MainViewModel(settings);
            var window = new MainWindow {
                DataContext = mainViewModel
            };
            window.Closing += OnMainWindowClosing;
            window.Closed += OnMainWindowClosed;

            serverThread.Start();

            MainWindow = window;
            MainWindow.Show();
        }

        private IHostBuilder CreateHostBuilder(string[] args)
        {
            return Host.CreateDefaultBuilder(args)
                .ConfigureWebHostDefaults(
                    webBuilder => {
                        webBuilder.ConfigureServices(services => {
                            services.AddControllers();
                            services.AddSingleton(settings);
                            services.AddSingleton<ILastfmRequestLogger>(mainViewModel!);
                        });
                        webBuilder.Configure(
                            app => {
                                app.UseRouting();
                                app.UseEndpoints(endpoints => {
                                    endpoints.MapControllers();
                                });
                            });
                        webBuilder.ConfigureLogging(
                            x => {
                                x.ClearProviders();
                                x.AddProvider(new MyLoggerProvider(mainViewModel!));
                            });
                        //webBuilder.UseStartup(
                        //    x => new Startup(x.Configuration));
                    });
        }

        private class MyLoggerProvider : ILoggerProvider
        {
            private readonly ILogItemSink logSink;

            public MyLoggerProvider(ILogItemSink logSink)
            {
                this.logSink = logSink;
            }

            public void Dispose()
            {
            }

            public ILogger CreateLogger(string categoryName)
            {
                return new MyLogger(logSink);
            }
        }

        private class MyLogger : ILogger
        {
            private readonly ILogItemSink logSink;

            public MyLogger(ILogItemSink logSink)
            {
                this.logSink = logSink;
            }

            public IDisposable BeginScope<TState>(TState state)
            {
                return new Scope();
            }

            private class Scope : IDisposable
            {
                public void Dispose()
                { }
            }

            public bool IsEnabled(LogLevel logLevel)
            {
                return true;
            }

            public void Log<TState>(
                LogLevel logLevel, EventId eventId, TState state, Exception exception,
                Func<TState, Exception, string> formatter)
            {
                string message = formatter(state, exception);
                logSink.Log(new LogItem(logLevel, eventId, message));
            }
        }

        private void OnMainWindowClosing(object sender, CancelEventArgs e)
        {
        }

        private async void OnMainWindowClosed(object? sender, EventArgs args)
        {
            if (host != null)
                await host.StopAsync();
            Shutdown();
        }
    }

    public interface ILogItemSink
    {
        void Log(LogItem item);
    }

    public class LastmApiRequest
    {
        public LastmApiRequest(string method, Dictionary<string, StringValues> data, IActionResult result)
        {
            Method = method;
            Data = data;
            Result = result;
        }

        public string Method { get; }
        public IReadOnlyDictionary<string, StringValues> Data { get; }
        public IActionResult Result { get; }
        public string? DisplayResult => DumpResult(Result);

        private static string? DumpResult(object result)
        {
            switch (result) {
                case JsonResult json:
                    return JsonSerializer.Serialize(json.Value);
                case BadRequestObjectResult badRequest:
                    return "400: " + DumpResult(badRequest.Value);
                case null:
                    return "<null>";
                default:
                    return result.ToString();
            }
        }

        public string EncodedData
        {
            get
            {
                var entries = Data.OrderBy(x => x.Key).Select(x => {
                    if (x.Value.Count > 2)
                        return x.Key + "= [" + string.Join("; ", x.Value) + "]";
                    if (x.Value.Count == 1)
                        return x.Key + "=" + x.Value[0];
                    return x.Key;
                });

                return string.Join("; ", entries);
            }
        }
    }

    public interface ILastfmRequestLogger
    {
        void Log(LastmApiRequest request);
    }

    public class ApiStubSettings
    {
    }
}
