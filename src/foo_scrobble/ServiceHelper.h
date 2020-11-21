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
    {}
};

template<typename T1, typename T2, typename T3>
class service_multi_inherit3
    : public T1
    , public T2
    , public T3
{
    using self_t = service_multi_inherit3<T1, T2, T3>;

public:
    static bool handle_service_query(service_ptr& out, GUID const& guid, self_t* in)
    {
        return service_base::handle_service_query(out, guid, static_cast<T1*>(in)) ||
               service_base::handle_service_query(out, guid, static_cast<T2*>(in)) ||
               service_base::handle_service_query(out, guid, static_cast<T3*>(in));
    }

    service_base* as_service_base() { return T1::as_service_base(); }

    // Obscure service_base methods from both so calling myclass->service_query() works
    // like it should
    virtual int service_release() noexcept = 0;
    virtual int service_add_ref() noexcept = 0;
    virtual bool service_query(service_ptr& p_out, GUID const& p_guid) = 0;
};

} // namespace foo_scrobble
