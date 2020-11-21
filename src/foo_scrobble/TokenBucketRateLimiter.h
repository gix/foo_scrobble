#pragma once
#include <algorithm>
#include <chrono>

namespace foo_scrobble
{

class TokenBucketRateLimiter
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = Clock::duration;

    TokenBucketRateLimiter(double tokensPerSecond, double burstCapacity)
        : tokensPerSecond_(tokensPerSecond)
        , burstCapacity_(burstCapacity)
        , tokens_(burstCapacity)
        , lastStep_(Clock::now())
    {}

    Duration Acquire(size_t n = 1) { return Acquire(Clock::now(), n); }

    Duration Acquire(Clock::time_point now, size_t n = 1)
    {
        if (n == 0)
            return Duration::zero();

        Advance(now);
        if (n > tokens_)
            return DurationFromTokens(n - tokens_);

        tokens_ -= n;
        return Duration::zero();
    }

private:
    double TokensFromDuration(Duration duration) const
    {
        using namespace std::chrono;
        return duration_cast<seconds>(duration).count() * tokensPerSecond_;
    }

    Duration DurationFromTokens(double tokens) const
    {
        using namespace std::chrono;
        return duration_cast<Duration>(duration<double>(tokens / tokensPerSecond_));
    }

    void Advance(Clock::time_point const now)
    {
        auto const elapsed = now - lastStep_;

        if (elapsed > Duration::zero()) {
            tokens_ = std::min(tokens_ + TokensFromDuration(elapsed), burstCapacity_);
            lastStep_ = now;
        }
    }

    double tokensPerSecond_;
    double burstCapacity_;
    double tokens_;
    Clock::time_point lastStep_;
};

} // namespace foo_scrobble
