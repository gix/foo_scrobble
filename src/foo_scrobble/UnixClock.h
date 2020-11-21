#pragma once
#include <chrono>
#include <ctime>

namespace foo_scrobble
{

//! Chrono clock for Unix time (epoch 1970-01-01T00:00:00Z).
struct unix_clock
{
    using rep = long long;
    using period = std::ratio<1>;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<unix_clock>;
    static constexpr bool is_steady = false;

    static time_point now() noexcept
    {
        time_t tm;
        time(&tm);
        return time_point{duration{tm}};
    }

    static time_t to_time_t(time_point const& time) noexcept
    {
        return static_cast<time_t>(time.time_since_epoch().count());
    }

    static time_point from_time_t(time_t tm) noexcept
    {
        return time_point{duration{tm}};
    }
};

} // namespace foo_scrobble
