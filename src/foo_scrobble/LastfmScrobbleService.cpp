#include "ScrobbleService.h"

#include "AsyncHelper.h"
#include "ScrobbleCache.h"
#include "ScrobbleConfig.h"
#include "ServiceHelper.h"
#include "TokenBucketRateLimiter.h"
#include "Track.h"
#include "WebService.h"

#include <string_view>
#include <winhttp.h>

using namespace std::chrono;
using namespace std::string_view_literals;

namespace foo_scrobble
{
namespace
{

using ::operator<<;
using foo_scrobble::operator<<;

pfc::string_base& operator<<(pfc::string_base& fmt, std::string_view str)
{
    fmt.add_string(str.data(), str.length());
    return fmt;
}

class LastfmScrobbleService
    : public ScrobbleService
    , public ScrobbleConfigNotify
    , public init_stage_callback
{
public:
    virtual ~LastfmScrobbleService() = default;

    virtual void ScrobbleAsync(Track track) override;
    virtual void SendNowPlayingAsync(Track const& track) override;
    virtual void Shutdown() override;

    virtual void on_init_stage(t_uint32 stage) override
    {
        if (stage == init_stages::after_ui_init)
            SetSessionKey(Config.SessionKey);
    }

    virtual void OnConfigChanged() override { SetSessionKey(Config.SessionKey); }

private:
    enum class State
    {
        UnauthenticatedIdle,
        AuthenticatedIdle,
        AwaitingResponse,
        Sleeping,
        Suspended,
        ShuttingDown,
        ShutDown,
    };

    void OnScrobbleResponse(outcome<void> result);
    void OnNowPlayingResponse(outcome<void> result);
    void OnWakeUp();

    void LogResponse(std::string_view task, outcome<void> const& result);
    void HandleResponseStatus(lastfm::Status status);

    void ProcessLocked();
    void PauseProcessing(duration<int, std::milli> delay);
    void SetSessionKey(pfc::string_base const& newSessionKey);

    static constexpr double ComputeBurstCapacity(double tokensPerSecond)
    {
        // Burst capacity to honor the given TPS over 5mins. This would
        // theoretically allow bursts up to 5min*TPS but due to the constant
        // token generation this can exceed the upper limit. Halving the burst
        // ensures we never exceed it.
        return (5 * 60) * tokensPerSecond / 2.0;
    }

    using ExclusiveLock = std::scoped_lock<std::mutex>;
    std::mutex mutex_;
    std::condition_variable cv_;

    WebService webService_{lastfm::ApiKey, lastfm::Secret};
    TokenBucketRateLimiter rateLimiter_{5.0, ComputeBurstCapacity(5.0)};

    State state_ = State::UnauthenticatedIdle;
    ScrobbleCache& scrobbleCache_{ScrobbleCache::Get()};
    pplx::cancellation_token_source cts_;
    size_t pendingSubmissionSize_ = 0;
    Track pendingNowPlaying_;
};

void LastfmScrobbleService::ScrobbleAsync(Track track)
{
    ExclusiveLock lock(mutex_);
    scrobbleCache_.Add(std::move(track));

    switch (state_) {
    case State::AuthenticatedIdle:
        break;

    case State::UnauthenticatedIdle:
        FB2K_console_formatter() << "foo_scrobble: Queuing scrobble (Unauthenticated)";
        return;
    case State::Suspended:
        FB2K_console_formatter() << "foo_scrobble: Queuing scrobble (Invalid API key)";
        return;
    case State::Sleeping:
        FB2K_console_formatter() << "foo_scrobble: Queuing scrobble (Sleeping)";
        return;
    case State::AwaitingResponse:
    case State::ShuttingDown:
    case State::ShutDown:
        return;
    }

    ProcessLocked();
}

void LastfmScrobbleService::SendNowPlayingAsync(Track const& track)
{
    ExclusiveLock lock(mutex_);
    pendingNowPlaying_ = track;

    switch (state_) {
    case State::UnauthenticatedIdle:
    case State::Sleeping:
    case State::Suspended:
    case State::ShuttingDown:
    case State::ShutDown:
    case State::AwaitingResponse:
        return;
    case State::AuthenticatedIdle:
        break;
    }

    ProcessLocked();
}

void LastfmScrobbleService::SetSessionKey(pfc::string_base const& newSessionKey)
{
    ExclusiveLock lock(mutex_);
    webService_.SetSessionKey({newSessionKey.get_ptr(), newSessionKey.get_length()});

    if (state_ == State::UnauthenticatedIdle) {
        state_ = State::AuthenticatedIdle;
        ProcessLocked();
    }
}

void LastfmScrobbleService::Shutdown()
{
    std::unique_lock<std::mutex> lock(mutex_);

    switch (state_) {
    case State::ShuttingDown:
        return;

    case State::UnauthenticatedIdle:
    case State::AuthenticatedIdle:
    case State::Suspended:
    case State::Sleeping:
    case State::ShutDown:
        state_ = State::ShutDown;
        return;

    case State::AwaitingResponse:
        state_ = State::ShuttingDown;
        break;
    }

    if (!cv_.wait_for(lock, milliseconds(500),
                      [&]() { return state_ == State::ShutDown; })) {
        cts_.cancel();
        state_ = State::ShutDown;
    }
}

void LastfmScrobbleService::OnWakeUp()
{
    ExclusiveLock lock(mutex_);

    switch (state_) {
    case State::ShuttingDown:
    case State::ShutDown:
        return;

    case State::UnauthenticatedIdle:
    case State::Suspended:
    case State::AwaitingResponse:
    case State::AuthenticatedIdle:
        assert(false);
        return;

    case State::Sleeping:
        state_ = State::AuthenticatedIdle;
        ProcessLocked();
        break;
    }
}

void LastfmScrobbleService::ProcessLocked()
{
    assert(state_ == State::AuthenticatedIdle);

    if (!pendingNowPlaying_.IsValid() && scrobbleCache_.IsEmpty())
        return; // No work to do.

    auto delay = duration_cast<duration<int, std::milli>>(rateLimiter_.Acquire());
    if (delay.count() != 0) {
        PauseProcessing(delay);
        return;
    }

    if (pendingNowPlaying_.IsValid()) {
        state_ = State::AwaitingResponse;
        auto task = webService_.SendNowPlaying(pendingNowPlaying_, cts_.get_token());
        task.then([&](outcome<void> result) { OnNowPlayingResponse(std::move(result)); });
        return;
    }

    if (scrobbleCache_.Count() == 1) {
        pendingSubmissionSize_ = 1;

        FB2K_console_formatter() << "foo_scrobble: Submitting track";

        state_ = State::AwaitingResponse;
        auto task = webService_.Scrobble(scrobbleCache_[0], cts_.get_token());
        task.then([&](outcome<void> result) { OnScrobbleResponse(std::move(result)); });
    } else if (scrobbleCache_.Count() > 1) {
        size_t const maxScrobblesPerRequest = 50;
        size_t const batchSize = std::min(scrobbleCache_.Count(), maxScrobblesPerRequest);

        pendingSubmissionSize_ = batchSize;
        auto request = webService_.CreateScrobbleRequest();
        for (size_t i = 0; i < batchSize; ++i)
            request.AddTrack(scrobbleCache_[i]);

        FB2K_console_formatter() << "foo_scrobble: Submitting " << batchSize << " of "
                                 << scrobbleCache_.Count() << " cached tracks";

        state_ = State::AwaitingResponse;
        auto task = webService_.Scrobble(std::move(request), cts_.get_token());
        task.then([&](outcome<void> result) { OnScrobbleResponse(std::move(result)); });
    }
}

void LastfmScrobbleService::PauseProcessing(duration<int, std::milli> delay)
{
    state_ = State::Sleeping;
    create_delay(delay, cts_.get_token()).then([&]() { OnWakeUp(); });
}

lastfm::Status MapException(web::http::http_exception const& ex)
{
    switch (ex.error_code().value()) {
    case ERROR_WINHTTP_TIMEOUT:
    case ERROR_WINHTTP_NAME_NOT_RESOLVED:
    case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_CONNECTION_ERROR:
        return lastfm::Status::ConnectionError;
    default:
        return lastfm::Status::InternalError;
    }
}

lastfm::Status AsStatus(outcome<void> const& result)
{
    if (result)
        return lastfm::Status::Success;
    if (result.has_error() && result.error().category() == lastfm::webservice_category())
        return static_cast<lastfm::Status>(result.error().value());

    try {
        std::rethrow_exception(result.exception());
    } catch (web::http::http_exception const& ex) {
        return MapException(ex);
    } catch (...) {
        return lastfm::Status::InternalError;
    }
}

void LastfmScrobbleService::OnScrobbleResponse(outcome<void> result)
{
    {
        ExclusiveLock lock(mutex_);

        LogResponse("Scrobbling"sv, result);

        auto const status = AsStatus(result);

        switch (status) {
        case lastfm::Status::Success:
        case lastfm::Status::InvalidParameters: // Invalid entry so retrying is of no use.
            scrobbleCache_.Evict(pendingSubmissionSize_);
            pendingSubmissionSize_ = 0;
            break;

        default:
            // Retry.
            break;
        }

        HandleResponseStatus(status);
    }

    cv_.notify_all();
}

void LastfmScrobbleService::OnNowPlayingResponse(outcome<void> result)
{
    {
        ExclusiveLock lock(mutex_);

        LogResponse("NowPlaying notification"sv, result);

        auto const status = AsStatus(result);

        switch (status) {
        case lastfm::Status::InvalidSessionKey:
        case lastfm::Status::ServiceOffline:
        case lastfm::Status::ServiceTemporarilyUnavailable:
        case lastfm::Status::ConnectionError:
            // The only cases that make sense to retry later.
            break;

        case lastfm::Status::Success:
        default:
            pendingNowPlaying_ = {};
        }

        HandleResponseStatus(status);
    }

    cv_.notify_all();
}

void LastfmScrobbleService::HandleResponseStatus(lastfm::Status status)
{
    if (state_ == State::AwaitingResponse) {
        switch (status) {
        default:
            state_ = State::AuthenticatedIdle;
            ProcessLocked();
            break;

        case lastfm::Status::AuthenticationFailed:
        case lastfm::Status::InvalidSessionKey:
            state_ = State::UnauthenticatedIdle;
            break;

        case lastfm::Status::InvalidAPIKey:
        case lastfm::Status::SuspendedAPIKey:
            state_ = State::Suspended;
            break;

        case lastfm::Status::ServiceOffline:
        case lastfm::Status::ServiceTemporarilyUnavailable:
            PauseProcessing(minutes(2));
            break;

        case lastfm::Status::RateLimitExceeded:
            PauseProcessing(minutes(2));
            break;

        case lastfm::Status::InvalidResponse:
        case lastfm::Status::InternalError:
            PauseProcessing(minutes(1));
            break;

        case lastfm::Status::ConnectionError:
            PauseProcessing(seconds(30));
            break;
        }
    } else {
        assert(state_ == State::ShuttingDown || state_ == State::ShutDown);
        state_ = State::ShutDown;
    }
}

void LastfmScrobbleService::LogResponse(std::string_view task,
                                        outcome<void> const& result)
{
    if (result.has_value())
        return;

    if (result.has_exception()) {
        FB2K_console_formatter()
            << "foo_scrobble: " << task << " failed (" << result << ")";
        return;
    }

    if (result.error().category() != lastfm::webservice_category()) {
        FB2K_console_formatter()
            << "foo_scrobble: " << task << " failed (" << result << ")";
        return;
    }

    auto const status = static_cast<lastfm::Status>(result.error().value());

    switch (status) {
    case lastfm::Status::Success:
        break;

    case lastfm::Status::InvalidService:
    case lastfm::Status::InvalidMethod:
    case lastfm::Status::InvalidMethodSignature:
    case lastfm::Status::InvalidFormat:
    case lastfm::Status::InvalidParameters:
    case lastfm::Status::InvalidResourceSpecified:
    case lastfm::Status::OperationFailed:
    case lastfm::Status::TokenNotAuthorized:
        FB2K_console_formatter()
            << "foo_scrobble: " << task << " failed (" << status << ")";
        break;

    case lastfm::Status::AuthenticationFailed:
    case lastfm::Status::InvalidSessionKey:
        FB2K_console_formatter()
            << "foo_scrobble: " << task << " failed (" << status << ")";
        break;

    case lastfm::Status::InvalidAPIKey:
    case lastfm::Status::SuspendedAPIKey:
        FB2K_console_formatter()
            << "foo_scrobble: " << task << " failed (" << status << ")";
        break;

    case lastfm::Status::ServiceOffline:
    case lastfm::Status::ServiceTemporarilyUnavailable:
    case lastfm::Status::ConnectionError:
        FB2K_console_formatter()
            << "foo_scrobble: " << task << " failed (" << status << ")";
        break;

    case lastfm::Status::RateLimitExceeded:
        FB2K_console_formatter()
            << "foo_scrobble: " << task << " failed (" << status << ")";
        break;

    case lastfm::Status::InvalidResponse:
        FB2K_console_formatter()
            << "foo_scrobble: " << task << " (Invalid service response)";
        break;

    case lastfm::Status::InternalError:
        FB2K_console_formatter()
            << "foo_scrobble: " << task << " (Internal foo_scrobble error)";
        break;
    }
}

} // namespace

static service_factory_single_v_t<LastfmScrobbleService, ScrobbleService,
                                  ScrobbleConfigNotify, init_stage_callback>
    g_LastfmScrobblerService;

} // namespace foo_scrobble
