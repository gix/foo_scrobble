#pragma once
#include "Track.h"

namespace foo_scrobble
{

class NOVTABLE ScrobbleService : public service_base
{
public:
    virtual void ScrobbleAsync(Track track) = 0;
    virtual void SendNowPlayingAsync(Track const& track) = 0;
    virtual void Shutdown() = 0;

    FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(ScrobbleService)
};

} // namespace foo_scrobble
