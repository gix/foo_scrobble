namespace LastfmApiStub.Controllers
{
    using System;
    using System.IO;
    using System.Linq;
    using System.Net.Mime;
    using System.Security.Cryptography;
    using System.Text;
    using System.Text.Json;
    using System.Threading;
    using System.Threading.Tasks;
    using Microsoft.AspNetCore.Http;
    using Microsoft.AspNetCore.Mvc;
    using Microsoft.AspNetCore.Mvc.Infrastructure;
    using Microsoft.Extensions.Logging;

    [ApiController]
    [Route("")]
    public class LastfmApiController : Controller
    {
        private readonly ILogger logger;
        private readonly ILastfmRequestLogger requestLogger;

        public LastfmApiController(
            ILogger<LastfmApiController> logger, ILastfmRequestLogger requestLogger)
        {
            this.logger = logger;
            this.requestLogger = requestLogger;
        }

        [HttpPost]
        public IActionResult Index([FromForm] IFormCollection data)
        {
            var now = DateTime.Now;
            var result = HandleRequest(data);

            //logger.LogWarning("{0} {1}\nQuery: {2}\nValue:\n{3}\n{4}",
            //                  HttpContext.Request.Method,
            //                  HttpContext.Request.GetDisplayUrl(),
            //                  HttpContext.Request.QueryString,
            //                  string.Join("", values),
            //                  DumpResult(result));

            requestLogger.Log(new LastmApiRequest(
                now,
                data["method"],
                data.ToDictionary(x => x.Key, x => x.Value),
                result));

            return result;
        }

        private static string? DumpResult(object? result)
        {
            return result switch {
                JsonResult json => DumpObject(json.Value),
                BadRequestObjectResult badRequest => "400: " + DumpResult(badRequest.Value),
                null => "<null>",
                _ => result.ToString()
            };
        }

        private static string DumpObject(object? value)
        {
            return JsonSerializer.Serialize(value);
        }

        private class EncodingErrorResult : StatusCodeResult
        {
            public EncodingErrorResult() : base(200)
            { }

            public override void ExecuteResult(ActionContext context)
            {
                base.ExecuteResult(context);

                var windows1252 = CodePagesEncodingProvider.Instance.GetEncoding(1252)
                    ?? throw new InvalidOperationException("CodePage 1252 not available");

                context.HttpContext.Response.ContentType = "application/json";

                string response = "{\"status\":\"ök\"}";

                Stream stream = context.HttpContext.Response.Body;
                stream.Write(windows1252.GetBytes(response));
            }
        }

        private IActionResult HandleRequest(IFormCollection data)
        {
            string method = data["method"];

            if (requestLogger.SlowResponseTime != null) {
                Thread.Sleep(TimeSpan.FromMilliseconds(requestLogger.SlowResponseTime.Value));
            }

            if (!CheckSignature(data)) {
                return BadRequest(
                    MakeErrorResult(
                        LastfmStatus.InvalidMethodSignature, "Invalid method signature supplied."));
            }

            if (requestLogger.ErrorResponseKind != null) {
                if (requestLogger.ErrorResponseKind > 0) {
                    var status = (LastfmStatus)requestLogger.ErrorResponseKind;
                    return BadRequest(MakeErrorResult(status, status.ToString()));
                }

                return requestLogger.ErrorResponseKind switch {
                    ErrorResponseKind.InvalidContentType => Content("Bogus", "text/plain"),
                    ErrorResponseKind.EncodingError => new EncodingErrorResult(),
                    _ => throw new ArgumentOutOfRangeException()
                };
            }

            switch (method) {
                case "track.updateNowPlaying":
                    break;
                case "track.scrobble":
                    break;

                default:
                    return BadRequest(
                        MakeErrorResult(LastfmStatus.InvalidMethod, "No method with that name in this package."));
            }

            return Json(new { status = "ok" });
        }

        private static object MakeErrorResult(LastfmStatus error, string message)
        {
            return new { error, message };
        }

        private static bool CheckSignature(IFormCollection data)
        {
            if (!data.ContainsKey("api_sig"))
                return false;

            string s = string.Join("", data.Where(x => x.Key != "api_sig" && x.Key != "format" && x.Key != "callback")
                .OrderBy(x => x.Key, StringComparer.Ordinal)
                .Select(x => x.Key + x.Value));
            s += "1a88014994509aeab650d46830ff7d7c";

            byte[] actualHash = ToBytes(data["api_sig"].ToString());
            byte[] expectedHash = MD5.Create().ComputeHash(Encoding.UTF8.GetBytes(s));

            return actualHash.SequenceEqual(expectedHash);
        }

        private static byte[] ToBytes(string str)
        {
            if (str.Length % 2 != 0)
                throw new ArgumentException("String must have even length.", nameof(str));

            var bytes = new byte[str.Length / 2];
            for (int i = 0; i < bytes.Length; ++i) {
                bytes[i] = Convert.ToByte(str.Substring(i * 2, 2), 16);
            }

            return bytes;
        }
    }

    internal enum LastfmStatus
    {
        Success = 0,

        //! The service does not exist.
        InvalidService = 2,

        //! No method with that name in this package.
        InvalidMethod = 3,

        //! No permissions to access the service.
        AuthenticationFailed = 4,

        //! The service does not exist in that format.
        InvalidFormat = 5,

        //! Request is missing a required parameter.
        InvalidParameters = 6,

        //! Invalid resource specified.
        InvalidResourceSpecified = 7,

        //! Something else went wrong.
        OperationFailed = 8,

        //! Please re - authenticate.
        InvalidSessionKey = 9,

        //! You must be granted a valid key by last.fm.
        InvalidAPIKey = 10,

        //! The service is temporarily offline. Try again later.
        ServiceOffline = 11,

        //! Invalid method signature supplied.
        InvalidMethodSignature = 13,

        TokenNotAuthorized = 14,

        //! There was a temporary error processing your request. Try again later.
        ServiceTemporarilyUnavailable = 16,

        //! Access for the API account has been suspended.
        SuspendedAPIKey = 26,

        //! Your IP has made too many requests in a short period.
        RateLimitExceeded = 29,

        InvalidResponse = -1,
        InternalError = -2,
    }
}
