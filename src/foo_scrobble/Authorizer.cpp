#include "Authorizer.h"
#include "WebService.h"

#include <coroutine>
#include <pplawait.h>

namespace foo_scrobble
{

static Authorizer::State FromSessionKey(pfc::string_base const& sessionKey)
{
    if (sessionKey && strlen(sessionKey) != 0)
        return Authorizer::State::Authorized;

    return Authorizer::State::Unauthorized;
}

static void OpenAuthConfirmationInBrowser(std::string const& token)
{
    web::http::uri_builder builder(lastfm::AuthUrl);
    builder.append_query(L"api_key", lastfm::ApiKey);
    builder.append_query(L"token", utility::conversions::to_utf16string(token));
    std::wstring url = builder.to_string();

    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

Authorizer::Authorizer(pfc::string_base const& sessionKey)
    : state_(FromSessionKey(sessionKey))
    , sessionKey_(sessionKey)
{
}

Authorizer::State Authorizer::GetState() const { return state_; }

void Authorizer::ClearAuth()
{
    sessionKey_ = "";
    state_ = State::Unauthorized;
}

void Authorizer::CancelAuth()
{
    cts_.cancel();
    cts_ = pplx::cancellation_token_source();
    authToken_.clear();
    state_ = FromSessionKey(sessionKey_);
}

pplx::task<bool> Authorizer::RequestAuthAsync()
{
    if (state_ == State::RequestingAuth)
        co_return false;

    state_ = State::RequestingAuth;
    console::info("foo_scrobble: Requesting auth token");

    WebService service(lastfm::ApiKey, lastfm::Secret);
    outcome<std::string> result = co_await service.GetAuthToken(cts_.get_token());

    if (!result) {
        FB2K_console_formatter()
            << "foo_scrobble: Failed to get auth token (" << result << ")";
        state_ = FromSessionKey(sessionKey_);
        co_return false;
    }

    std::string& authToken = result.value();
    FB2K_console_formatter() << "foo_scrobble: Received auth token: "
                             << authToken.c_str();
    OpenAuthConfirmationInBrowser(authToken);

    authToken_ = std::move(authToken);
    state_ = State::WaitingForApproval;
    co_return true;
}

pplx::task<bool> Authorizer::CompleteAuthAsync()
{
    if (state_ == State::CompletingAuth)
        co_return false;

    state_ = State::CompletingAuth;
    console::info("foo_scrobble: Requesting session key");

    WebService service(lastfm::ApiKey, lastfm::Secret);
    outcome<std::string> result =
        co_await service.GetSessionKey(authToken_.c_str(), cts_.get_token());

    if (!result) {
        FB2K_console_formatter()
            << "foo_scrobble: Failed to get session key (" << result << ")";
        state_ = FromSessionKey(sessionKey_);
        co_return false;
    }

    std::string& sessionKey = result.value();
    FB2K_console_formatter() << "foo_scrobble: New session key: " << sessionKey.c_str();

    sessionKey_ = sessionKey.c_str();
    authToken_.clear();

    state_ = FromSessionKey(sessionKey_);
    co_return true;
}

} // namespace foo_scrobble
