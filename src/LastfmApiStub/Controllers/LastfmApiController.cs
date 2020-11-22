namespace LastfmApiStub.Controllers
{
    using System;
    using System.Linq;
    using System.Security.Cryptography;
    using System.Text;
    using System.Text.Json;
    using Microsoft.AspNetCore.Http;
    using Microsoft.AspNetCore.Http.Extensions;
    using Microsoft.AspNetCore.Mvc;
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
            var result = HandleRequest(data);
            var values = data
                .OrderBy(x => x.Key, StringComparer.Ordinal)
                .Select(x => $"  {x.Key}: {x.Value}\n");

            //logger.LogWarning("{0} {1}\nQuery: {2}\nValue:\n{3}\n{4}",
            //                  HttpContext.Request.Method,
            //                  HttpContext.Request.GetDisplayUrl(),
            //                  HttpContext.Request.QueryString,
            //                  string.Join("", values),
            //                  DumpResult(result));

            requestLogger.Log(new LastmApiRequest(
                data["method"],
                data.ToDictionary(x => x.Key, x => x.Value),
                result));

            return result;
        }

        private static string? DumpResult(object result)
        {
            switch (result) {
                case JsonResult json:
                    return DumpObject(json.Value);
                case BadRequestObjectResult badRequest:
                    return "400: " + DumpResult(badRequest.Value);
                case null:
                    return "<null>";
                default:
                    return result.ToString();
            }
        }

        private static string DumpObject(object value)
        {
            return JsonSerializer.Serialize(value);
        }

        private IActionResult HandleRequest(IFormCollection data)
        {
            string method = data["method"];

            if (!CheckSignature(data)) {
                return BadRequest(
                    MakeErrorResult(
                        LastfmStatus.InvalidMethodSignature, "Invalid method signature supplied."));
            }

            //if (counter++ % 2 == 0) {
            //    return new JsonResult(
            //        MakeErrorResult(Status.OperationFailed, "Failed")) {
            //        StatusCode = 500
            //    };
            //}

            switch (method) {
                case "track.updateNowPlaying":
                    break;
                case "track.scrobble":
                    //Thread.Sleep(1500);
                    break;

                default:
                    return BadRequest(
                        MakeErrorResult(LastfmStatus.InvalidMethod, "No method with that name in this package."));
            }

            return Json(new { status = "ok" });
        }

        private object MakeErrorResult(LastfmStatus error, string message)
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
                throw new ArgumentException();

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
