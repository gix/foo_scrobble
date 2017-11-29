#pragma once
#include "Outcome.h"
#include "OutcomeCompat.h"
#include "fb2ksdk.h"

#include <map>
#include <system_error>

#include <cpprest/http_client.h>

namespace lastfm
{

wchar_t const* const AuthUrl = L"http://www.last.fm/api/auth";
extern char const* const ApiKey;
extern char const* const Secret;

enum class Status
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
    ConnectionError = -3,
};

} // namespace lastfm

namespace std
{
template<>
struct is_error_code_enum<lastfm::Status> : true_type
{
};

template<>
struct is_error_condition_enum<lastfm::Status> : true_type
{
};
} // namespace std

namespace lastfm
{

class ErrorCategory : public std::error_category
{
public:
    virtual const char* name() const noexcept override { return "last.fm api"; }

    virtual std::string message(int ev) const override
    {
        switch (static_cast<Status>(ev)) {
        case Status::Success:
            return "Success";
        case Status::InvalidService:
            return "The service does not exist.";
        case Status::InvalidMethod:
            return "No method with that name in this package.";
        case Status::AuthenticationFailed:
            return "No permissions to access the service.";
        case Status::InvalidFormat:
            return "The service does not exist in that format.";
        case Status::InvalidParameters:
            return "Request is missing a required parameter.";
        case Status::InvalidResourceSpecified:
            return "Invalid resource specified.";
        case Status::OperationFailed:
            return "Something else went wrong.";
        case Status::InvalidSessionKey:
            return "Please re - authenticate.";
        case Status::InvalidAPIKey:
            return "You must be granted a valid key by last.fm.";
        case Status::ServiceOffline:
            return "The service is temporarily offline. Try again later.";
        case Status::InvalidMethodSignature:
            return "Invalid method signature supplied.";
        case Status::ServiceTemporarilyUnavailable:
            return "There was a temporary error processing your request. Try again later.";
        case Status::SuspendedAPIKey:
            return "Access for the API account has been suspended.";
        case Status::RateLimitExceeded:
            return "Your IP has made too many requests in a short period.";
        case Status::TokenNotAuthorized:
            return "Token has not been authorized.";
        case Status::InvalidResponse:
            return "Invalid response.";
        case Status::InternalError:
            return "Internal error.";
        case Status::ConnectionError:
            return "Connection error.";
        default:
            return "Unknown error";
        }
    }
};

inline std::error_category const& webservice_category()
{
    static ErrorCategory instance;
    return instance;
}

inline std::error_code make_error_code(Status st)
{
    return {static_cast<int>(st), webservice_category()};
}

} // namespace lastfm

namespace foo_scrobble
{

using OUTCOME_V2_NAMESPACE::outcome;

class Track;

class WebService
{
public:
    explicit WebService(char const* apiKey, char const* secret);

    void SetSessionKey(std::string_view newSessionKey)
    {
        sessionKey_ = std::move(newSessionKey);
    }

    pplx::task<outcome<std::string>>
    GetAuthToken(pplx::cancellation_token cancellationToken);

    pplx::task<outcome<std::string>>
    GetSessionKey(std::string_view authToken, pplx::cancellation_token cancellationToken);

    pplx::task<outcome<void>> SendNowPlaying(Track const& track,
                                             pplx::cancellation_token cancellationToken);

    pplx::task<outcome<void>> Scrobble(Track const& track,
                                       pplx::cancellation_token cancellationToken);

    class MapIndex
    {
        static constexpr size_t const MaxSize = 16;

    public:
        MapIndex() = default;

        MapIndex(std::string_view name)
            : length_(static_cast<uint8_t>(std::min(MaxSize, name.length())))
        {
            std::memcpy(name_, name.data(), length_);
        }

        MapIndex(std::string_view name, uint8_t index)
            : length_(static_cast<uint8_t>(std::min(MaxSize - 4, name.length())))
        {
            std::memcpy(name_, name.data(), length_);
            AddIndex(index);
        }

        char const* data() const noexcept { return name_; }
        size_t length() const noexcept { return length_; }
        std::string_view string() const noexcept { return {name_, length_}; }

        bool operator<(MapIndex const& other) const noexcept;

    private:
        void AddIndex(uint8_t index)
        {
            name_[length_++] = '[';
            if (index >= 10)
                name_[length_++] = ('0' + (index / 10));
            name_[length_++] = ('0' + (index % 10));
            name_[length_++] = ']';
        }

        char name_[MaxSize];
        uint8_t length_ = 0;
    };

    using ParamsMap = std::map<MapIndex, std::string>;

    class ScrobbleRequest
    {
    public:
        ScrobbleRequest(ScrobbleRequest&&) = default;
        ScrobbleRequest& operator=(ScrobbleRequest&&) = default;

        uint8_t TrackCount() const { return trackCount_; }
        bool AddTrack(Track const& track);

    private:
        ScrobbleRequest(ParamsMap params)
            : params_(std::move(params))
        {
        }

        ParamsMap TakeParams() { return std::move(params_); }

        ParamsMap params_;
        uint8_t trackCount_ = 0;

        friend class WebService;
    };

    ScrobbleRequest CreateScrobbleRequest();

    pplx::task<outcome<void>> Scrobble(ScrobbleRequest request,
                                       pplx::cancellation_token cancellationToken);

private:
    ParamsMap NewParams(std::string_view method) const;
    ParamsMap NewAuthedParams(std::string_view method) const;

    void SignRequestParams(ParamsMap& params);

    pplx::task<web::http::http_response> Post(ParamsMap const& params,
                                              pplx::cancellation_token cancellationToken)
    {
        return Request(web::http::methods::POST, params, cancellationToken);
    }

    pplx::task<web::http::http_response>
    Request(web::http::method method, ParamsMap const& params,
            pplx::cancellation_token cancellationToken);

    web::http::client::http_client client_;
    std::string_view const apiKey_;
    std::string_view const secret_;
    std::string sessionKey_;
};

using ::operator<<;

pfc::string_base& operator<<(pfc::string_base& os, lastfm::Status status);
pfc::string_base& operator<<(pfc::string_base& os, std::exception_ptr const& ptr);
pfc::string_base& operator<<(pfc::string_base& os, std::error_code const& ec);

template<typename R, typename S, typename E, typename P>
pfc::string_base& operator<<(pfc::string_base& os, outcome<R, S, E, P> const& result)
{
    if (result.has_exception())
        os << result.exception();
    else if (result.has_error())
        os << result.error();
    return os;
}

} // namespace foo_scrobble
