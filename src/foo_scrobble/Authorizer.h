#pragma once
#include "fb2ksdk.h"
#include <cpprest/asyncrt_utils.h>

namespace foo_scrobble
{

class Authorizer
{
public:
    enum class State
    {
        Unauthorized = 0,
        RequestingAuth,
        WaitingForApproval,
        CompletingAuth,
        Authorized,
    };

    explicit Authorizer(pfc::string_base const& sessionKey);

    State GetState() const;
    pfc::string8_fast GetSessionKey() const { return sessionKey_; }

    void ClearAuth();
    void CancelAuth();
    pplx::task<bool> RequestAuthAsync();
    pplx::task<bool> CompleteAuthAsync();

private:
    State state_ = State::Unauthorized;
    pplx::cancellation_token_source cts_;

    pfc::string8_fast sessionKey_;
    std::string authToken_;
};

} // namespace foo_scrobble
