#include "ScrobbleConfig.h"

namespace foo_scrobble
{

// {FA44850D-3922-477B-92AE-7A07A17C7573}
static const GUID ScrobbleConfigId = {
    0xFA44850D, 0x3922, 0x477B, {0x92, 0xAE, 0x7A, 0x07, 0xA1, 0x7C, 0x75, 0x73}};

ScrobbleConfig::ScrobbleConfig()
    : cfg_var(ScrobbleConfigId)
    , EnableScrobbling(true)
    , EnableNowPlaying(true)
    , SubmitOnlyInLibrary(false)
    , SubmitDynamicSources(true)
    , ArtistMapping(DefaultArtistMapping)
    , TitleMapping(DefaultTitleMapping)
    , AlbumMapping(DefaultAlbumMapping)
    , AlbumArtistMapping(DefaultAlbumArtistMapping)
    , TrackNumberMapping(DefaultTrackNumberMapping)
    , MBTrackIdMapping(DefaultMBTrackIdMapping)
{
}

void ScrobbleConfig::get_data_raw(stream_writer* p_stream, abort_callback& p_abort)
{
    p_stream->write_lendian_t(Version, p_abort);

    p_stream->write_lendian_t(EnableScrobbling, p_abort);
    p_stream->write_lendian_t(EnableNowPlaying, p_abort);
    p_stream->write_lendian_t(SubmitOnlyInLibrary, p_abort);
    p_stream->write_lendian_t(SubmitDynamicSources, p_abort);

    p_stream->write_string(ArtistMapping, p_abort);
    p_stream->write_string(TitleMapping, p_abort);
    p_stream->write_string(AlbumMapping, p_abort);
    p_stream->write_string(AlbumArtistMapping, p_abort);
    p_stream->write_string(TrackNumberMapping, p_abort);
    p_stream->write_string(MBTrackIdMapping, p_abort);

    p_stream->write_string(SessionKey, p_abort);
}

void ScrobbleConfig::set_data_raw(stream_reader* p_stream, t_size /*p_sizehint*/,
                                  abort_callback& p_abort)
{
    unsigned version;
    p_stream->read_lendian_t(version, p_abort);
    if (version != Version)
        return;

    p_stream->read_lendian_t(EnableScrobbling, p_abort);
    p_stream->read_lendian_t(EnableNowPlaying, p_abort);
    p_stream->read_lendian_t(SubmitOnlyInLibrary, p_abort);
    p_stream->read_lendian_t(SubmitDynamicSources, p_abort);

    p_stream->read_string(ArtistMapping, p_abort);
    p_stream->read_string(TitleMapping, p_abort);
    p_stream->read_string(AlbumMapping, p_abort);
    p_stream->read_string(AlbumArtistMapping, p_abort);
    p_stream->read_string(TrackNumberMapping, p_abort);
    p_stream->read_string(MBTrackIdMapping, p_abort);

    p_stream->read_string(SessionKey, p_abort);

    std::pair<pfc::string8*, char const*> mappings[] = {
        {&ArtistMapping, DefaultArtistMapping},
        {&AlbumMapping, DefaultAlbumMapping},
        {&AlbumArtistMapping, DefaultAlbumArtistMapping},
        {&TitleMapping, DefaultTitleMapping},
        {&MBTrackIdMapping, DefaultMBTrackIdMapping},
    };

    static_api_ptr_t<titleformat_compiler> compiler;
    for (auto&& mapping : mappings) {
        titleformat_object::ptr obj;
        if (!compiler->compile(obj, mapping.first->c_str()))
            *mapping.first = mapping.second;
    }
}

// {7EEA8B2D-57EA-4CBC-97FB-DC2CEB1791C7}
GUID const ScrobbleConfigNotify::class_guid = {
    0x7EEA8B2D, 0x57EA, 0x4CBC, {0x97, 0xFB, 0xDC, 0x2C, 0xEB, 0x17, 0x91, 0xC7}};

ScrobbleConfig Config;

} // namespace foo_scrobble
