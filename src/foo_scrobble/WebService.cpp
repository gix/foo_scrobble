#include "WebService.h"

#include "Track.h"

#include <exception>
#include <experimental/resumable>
#include <string_view>

#include <cpprest/http_msg.h>
#include <cpprest/json.h>
#include <pplawait.h>

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace foo_scrobble
{

namespace
{

char const* const LowercaseHexChars = "0123456789abcdef";
char const* const UppercaseHexChars = "0123456789ABCDEF";

std::string ToLowercaseHex(hasher_md5_result const& result)
{
    std::string hash;
    for (char c : result.m_data) {
        hash += LowercaseHexChars[static_cast<unsigned char>(c) >> 4];
        hash += LowercaseHexChars[static_cast<unsigned char>(c) & 0xF];
    }

    return hash;
}

class FormDataBuilder
{
public:
    FormDataBuilder() = default;
    FormDataBuilder(size_t capacity) { encoded_.reserve(capacity); }

    void Append(std::string_view name, std::string_view value)
    {
        if (!encoded_.empty())
            encoded_.push_back('&');

        AppendEncoded(name);
        encoded_.push_back('=');
        AppendEncoded(value);
    }

    std::string ReleaseString() { return std::move(encoded_); }

private:
    static bool IsRawByte(char c)
    {
        return c == 0x2A /*'*'*/ || c == 0x2D /*'-'*/ || c == 0x2E /*'.'*/ ||
               (c >= 0x30 && c <= 0x39) /*0-9*/ || (c >= 0x41 && c <= 0x5A) /*A-Z*/ ||
               c == 0x5F /*'_'*/ || (c >= 0x61 && c <= 0x7A) /*a-z*/;
    }

    void AppendEncoded(std::string_view str)
    {
        // HTML5, 4.10.22.6 URL-encoded form data, Paragraph 4.5.
        for (char const c : str) {
            if (c == 0x20) {
                encoded_.push_back('+');
            } else if (IsRawByte(c)) {
                encoded_.push_back(c);
            } else {
                encoded_.push_back('%');
                encoded_.push_back(UppercaseHexChars[(c >> 4) & 0xF]);
                encoded_.push_back(UppercaseHexChars[c & 0xF]);
            }
        }
    }

    std::string encoded_;
};

bool IsExcludedFromSignature(std::string_view paramName)
{
    // The "format" and "callback" parameters are not part of signatures.
    return paramName == "format"sv || paramName == "callback"sv;
}

lastfm::Status ExtractError(web::json::value const& message)
{
    auto const errorKey = L"error"s;

    if (message.is_object() && message.has_field(errorKey)) {
        auto&& error = message.at(errorKey);
        if (error.is_integer())
            return static_cast<lastfm::Status>(error.as_integer());
    }

    return lastfm::Status::InvalidResponse;
}

int Compare(char const* str1, size_t length1, char const* str2, size_t length2) noexcept
{
    int const cmp = std::memcmp(str1, str2, std::min(length1, length2));
    if (cmp != 0)
        return cmp;
    if (length1 < length2)
        return -1;
    if (length1 > length2)
        return 1;
    return 0;
}

void FillParamsFromTrack(WebService::ParamsMap& params, Track const& track,
                         bool simple = false)
{
    if (!simple)
        params["timestamp"sv] =
            std::to_string(track.Timestamp.time_since_epoch().count());

    params["artist"sv] = track.Artist;
    params["track"sv] = track.Title;

    if (track.Duration > std::chrono::seconds::zero())
        params["duration"sv] =
            std::to_string(static_cast<int>(std::round(track.Duration.count())));

    if (!track.Album.is_empty()) {
        params["album"sv] = track.Album;
        if (!track.TrackNumber.is_empty())
            params["trackNumber"sv] = track.TrackNumber;
        if (!track.AlbumArtist.is_empty())
            params["albumArtist"sv] = track.AlbumArtist;
    }

    if (!track.MusicBrainzId.is_empty())
        params["mbid"sv] = track.MusicBrainzId;

    if (track.IsDynamic)
        params["chosenByUser"sv] = "0";
}

void FillParamsFromTrack(WebService::ParamsMap& params, Track const& track, uint8_t index)
{
    params[{"timestamp"sv, index}] =
        std::to_string(track.Timestamp.time_since_epoch().count());

    params[{"artist"sv, index}] = track.Artist;
    params[{"track"sv, index}] = track.Title;

    if (track.Duration > std::chrono::seconds::zero())
        params[{"duration"sv, index}] =
            std::to_string(static_cast<int>(std::round(track.Duration.count())));

    if (!track.Album.is_empty()) {
        params[{"album"sv, index}] = track.Album;
        if (!track.TrackNumber.is_empty())
            params[{"trackNumber"sv, index}] = track.TrackNumber;
        if (!track.AlbumArtist.is_empty())
            params[{"albumArtist"sv, index}] = track.AlbumArtist;
    }

    if (!track.MusicBrainzId.is_empty())
        params[{"mbid"sv, index}] = track.MusicBrainzId;

    if (track.IsDynamic)
        params[{"chosenByUser"sv, index}] = "0";
}

bool IsSuccess(web::http::http_response const& response)
{
    return response.status_code() == web::http::status_codes::OK;
}

#if _DEBUG
// wchar_t const* const ServiceBaseUrl = L"http://localhost:64660/";
wchar_t const* const ServiceBaseUrl = L"http://ws.audioscrobbler.com/2.0/";
#else
wchar_t const* const ServiceBaseUrl = L"http://ws.audioscrobbler.com/2.0/";
#endif

} // namespace

WebService::WebService(char const* apiKey, char const* secret)
    : client_(ServiceBaseUrl)
    , apiKey_(apiKey)
    , secret_(secret)
{
}

WebService::ParamsMap WebService::NewParams(std::string_view method) const
{
    ParamsMap params;
    params.insert({"api_key"sv, static_cast<std::string>(apiKey_)});
    params.insert({"format"sv, "json"});
    params.insert({"method"sv, static_cast<std::string>(method)});
    return params;
}

WebService::ParamsMap WebService::NewAuthedParams(std::string_view method) const
{
    auto params = NewParams(method);
    params.insert({"sk"sv, sessionKey_});
    return params;
}

void WebService::SignRequestParams(ParamsMap& params)
{
    static_api_ptr_t<hasher_md5> hasher;

    hasher_md5_state state = {};
    hasher->initialize(state);
    for (auto const& param : params) {
        if (IsExcludedFromSignature(param.first.string()))
            continue;

        hasher->process_string(state, param.first.data(), param.first.length());
        hasher->process_string(state, param.second.data(), param.second.length());
    }

    hasher->process_string(state, secret_.data(), secret_.length());

    params["api_sig"sv] = ToLowercaseHex(hasher->get_result(state));
}

pplx::task<outcome<std::string>>
WebService::GetAuthToken(pplx::cancellation_token cancellationToken)
{
    auto params = NewParams("auth.getToken");
    SignRequestParams(params);

    try {
        web::http::http_response response = co_await Post(params, cancellationToken);
        web::json::value msg = co_await response.extract_json();
        if (!IsSuccess(response))
            co_return ExtractError(msg);

        std::wstring token = msg.at(L"token").as_string();
        co_return utility::conversions::to_utf8string(token);
    } catch (...) {
        co_return std::current_exception();
    }
}

pplx::task<outcome<std::string>>
WebService::GetSessionKey(std::string_view authToken,
                          pplx::cancellation_token cancellationToken)
{
    auto params = NewParams("auth.getSession");
    params["token"sv] = authToken;
    SignRequestParams(params);

    try {
        web::http::http_response response = co_await Post(params, cancellationToken);
        web::json::value msg = co_await response.extract_json();
        if (!IsSuccess(response))
            co_return ExtractError(msg);

        std::wstring key = msg.at(L"session").at(L"key").as_string();
        co_return utility::conversions::to_utf8string(key);
    } catch (...) {
        co_return std::current_exception();
    }
}

pplx::task<outcome<void>>
WebService::SendNowPlaying(Track const& track, pplx::cancellation_token cancellationToken)
{
    auto params = NewAuthedParams("track.updateNowPlaying");
    FillParamsFromTrack(params, track, true);
    SignRequestParams(params);

    try {
        web::http::http_response response = co_await Post(params, cancellationToken);
        web::json::value msg = co_await response.extract_json();
        if (!IsSuccess(response))
            co_return ExtractError(msg);

        co_return success();
    } catch (...) {
        co_return std::current_exception();
    }
}

pplx::task<outcome<void>> WebService::Scrobble(Track const& track,
                                               pplx::cancellation_token cancellationToken)
{
    auto params = NewAuthedParams("track.scrobble");
    FillParamsFromTrack(params, track);
    SignRequestParams(params);

    try {
        web::http::http_response response = co_await Post(params, cancellationToken);
        web::json::value msg = co_await response.extract_json();
        if (!IsSuccess(response))
            co_return ExtractError(msg);

        co_return success();
    } catch (...) {
        co_return std::current_exception();
    }
}

bool WebService::MapIndex::operator<(MapIndex const& other) const noexcept
{
    return Compare(name_, length_, other.name_, other.length_) < 0;
}

WebService::ScrobbleRequest WebService::CreateScrobbleRequest()
{
    return ScrobbleRequest(NewAuthedParams("track.scrobble"));
}

bool WebService::ScrobbleRequest::AddTrack(Track const& track)
{
    FillParamsFromTrack(params_, track, trackCount_);
    ++trackCount_;
    return true;
}

pplx::task<outcome<void>> WebService::Scrobble(ScrobbleRequest request,
                                               pplx::cancellation_token cancellationToken)
{
    auto params = request.TakeParams();
    SignRequestParams(params);

    try {
        web::http::http_response response = co_await Post(params, cancellationToken);
        web::json::value msg = co_await response.extract_json();
        std::wstring str = co_await response.extract_string();
        if (!IsSuccess(response))
            co_return ExtractError(msg);

        co_return success();
    } catch (...) {
        co_return std::current_exception();
    }
}

pplx::task<web::http::http_response>
WebService::Request(web::http::method method, ParamsMap const& params,
                    pplx::cancellation_token cancellationToken)
{
    FormDataBuilder formData;
    for (auto const& param : params)
        formData.Append(param.first.string(), param.second);

    web::http::http_request req(method);
    req.set_body(formData.ReleaseString(), "application/x-www-form-urlencoded");

    return client_.request(req, cancellationToken);
}

pfc::string_base& operator<<(pfc::string_base& os, lastfm::Status status)
{
    os << make_error_code(status);
    return os;
}

static size_t TrimmedLength(std::string const& str)
{
    auto length = str.length();
    while (length > 0 && (str[length - 1] == '\r' || str[length - 1] == '\n'))
        --length;
    return length;
}

static size_t TrimmedLength(char const* str)
{
    if (!str)
        return 0;

    auto length = strlen(str);
    while (length > 0 && (str[length - 1] == '\r' || str[length - 1] == '\n'))
        --length;
    return length;
}

struct Trimmed
{
    char const* String;
};

static pfc::string_base& operator<<(pfc::string_base& os, Trimmed const& value)
{
    os.add_string(value.String, TrimmedLength(value.String));
    return os;
}

pfc::string_base& operator<<(pfc::string_base& os, std::exception_ptr const& ptr)
{
    try {
        std::rethrow_exception(ptr);
    } catch (web::http::http_exception const& ex) {
        auto const code = ex.error_code();
        os << Trimmed{ex.what()} << " [error code: " << code.value() << ", "
           << Trimmed{code.message().c_str()} << "]";
    } catch (std::exception const& ex) {
        os << Trimmed{ex.what()};
    } catch (...) {
        os << "<unknown non-standard exception>";
    }
    return os;
}

pfc::string_base& operator<<(pfc::string_base& os, std::error_code const& ec)
{
    os << "error: " << static_cast<int>(ec.value()) << ", ";
    std::string const& message = ec.message();
    os.add_string(message.c_str(), TrimmedLength(message));
    return os;
}

} // namespace foo_scrobble
