#include "ScrobbleCache.h"
#include "ScrobbleService.h"

using namespace foo_scrobble;

namespace
{

template<bool IsBE, typename Rep>
stream_reader_formatter<IsBE>& operator>>(stream_reader_formatter<IsBE>& stream,
                                          std::chrono::duration<Rep>& value)
{
    Rep rep;
    stream >> rep;
    value = {rep};
    return stream;
}

template<bool IsBE, typename Rep>
stream_writer_formatter<IsBE>& operator<<(stream_writer_formatter<IsBE>& stream,
                                          const std::chrono::duration<Rep>& value)
{
    stream << value.count();
    return stream;
}

template<bool IsBE, typename Clock>
stream_reader_formatter<IsBE>& operator>>(stream_reader_formatter<IsBE>& stream,
                                          std::chrono::time_point<Clock>& value)
{
    using Duration = typename std::chrono::time_point<Clock>::duration;
    Duration duration;
    stream >> duration;
    value = std::chrono::time_point<Clock>{Duration{duration}};
    return stream;
}

template<bool IsBE, typename Clock>
stream_writer_formatter<IsBE>& operator<<(stream_writer_formatter<IsBE>& stream,
                                          std::chrono::time_point<Clock> const& value)
{
    stream << value.time_since_epoch();
    return stream;
}

FB2K_STREAM_READER_OVERLOAD(Track)
{
    return stream >> value.Timestamp >> value.Artist >> value.Title >>
           value.AlbumArtist >> value.Album >> value.TrackNumber >> value.MusicBrainzId >>
           value.Duration >> value.IsDynamic;
}

FB2K_STREAM_WRITER_OVERLOAD(Track)
{
    return stream << value.Timestamp << value.Artist << value.Title << value.AlbumArtist
                  << value.Album << value.TrackNumber << value.MusicBrainzId
                  << value.Duration << value.IsDynamic;
}

} // namespace

namespace foo_scrobble
{

// {5246EF91-2D84-4A21-A491-203DA82098CD}
static const GUID ScrobbleCacheGuid = {
    0x5246ef91, 0x2d84, 0x4a21, {0xa4, 0x91, 0x20, 0x3d, 0xa8, 0x20, 0x98, 0xcd}};

ScrobbleCache ScrobbleCache::instance_{ScrobbleCacheGuid};

void ScrobbleCache::get_data_raw(stream_writer* stream, abort_callback& abort)
{
    if (core_api::is_shutting_down())
        static_api_ptr_t<ScrobbleService>()->Shutdown();

    stream_writer_formatter<> out(*stream, abort);
    out << pfc::downcast_guarded<t_uint32>(tracks_.size());
    for (auto&& track : tracks_)
        out << track;
}

void ScrobbleCache::set_data_raw(stream_reader* stream, t_size /*sizehint*/,
                                 abort_callback& abort)
{
    try {
        stream_reader_formatter<> in(*stream, abort);
        t_uint32 size;
        in >> size;

        tracks_.resize(size);
        for (size_t i = 0; i < size; ++i)
            in >> tracks_[i];
    } catch (...) {
        tracks_.clear();
        throw;
    }
}

} // namespace foo_scrobble
