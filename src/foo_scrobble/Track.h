#pragma once
#include "UnixClock.h"

#include "fb2ksdk.h"
#include <chrono>

namespace foo_scrobble
{

class Track
{
public:
    unix_clock::time_point Timestamp;
    pfc::string8_fast Artist;
    pfc::string8_fast Title;
    pfc::string8_fast AlbumArtist;
    pfc::string8_fast Album;
    pfc::string8_fast TrackNumber;
    pfc::string8_fast MusicBrainzId;
    std::chrono::duration<double> Duration = std::chrono::duration<double>::zero();
    bool IsDynamic = false;

    bool IsValid() const { return Timestamp != unix_clock::time_point(); }
};

} // namespace foo_scrobble
