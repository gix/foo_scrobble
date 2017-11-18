#pragma once
#include "fb2ksdk.h"

namespace foo_scrobble
{

template<typename T, typename... S>
class service_factory_single_v_t
    : public service_impl_single_t<T>
    , public service_factory_single_ref_t<S>...
{
public:
    template<typename... Args>
    service_factory_single_v_t(Args&&... args)
        : service_impl_single_t<T>(std::forward<Args>(args)...)
        , service_factory_single_ref_t<S>(pfc::implicit_cast<S&>(*this))...
    {
    }
};

} // namespace foo_scrobble
