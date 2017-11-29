#pragma once
#include <outcome.hpp>
#include <ppltasks.h>

namespace Concurrency::details
{

template<typename R, typename S, typename E, typename P>
struct _ResultHolder<OUTCOME_V2_NAMESPACE::outcome<R, S, E, P>>
{
    using Type = OUTCOME_V2_NAMESPACE::outcome<R, S, E, P>;

    _ResultHolder() = default;

    Type Get() { return result_; }
    void Set(Type const& result) { result_ = result; }

private:
    Type result_{OUTCOME_V2_NAMESPACE::failure(S())};
};

} // Concurrency::details
