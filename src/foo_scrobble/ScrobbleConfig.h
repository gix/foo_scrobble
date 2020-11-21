#pragma once

namespace foo_scrobble
{

inline constexpr char const* DefaultArtistMapping = "[%artist%]";
inline constexpr char const* DefaultTitleMapping = "[%title%]";
inline constexpr char const* DefaultAlbumMapping = "[%album%]";
inline constexpr char const* DefaultAlbumArtistMapping = "[%album artist%]";
inline constexpr char const* DefaultTrackNumberMapping = "[%tracknumber%]";
inline constexpr char const* DefaultMBTrackIdMapping = "[%musicbrainz_trackid%]";

class ScrobbleConfig : public cfg_var
{
public:
    ScrobbleConfig();
    virtual ~ScrobbleConfig() = default;

    bool EnableScrobbling;
    bool EnableNowPlaying;
    bool SubmitOnlyInLibrary;
    bool SubmitDynamicSources;

    pfc::string8 ArtistMapping;
    pfc::string8 TitleMapping;
    pfc::string8 AlbumMapping;
    pfc::string8 AlbumArtistMapping;
    pfc::string8 TrackNumberMapping;
    pfc::string8 MBTrackIdMapping;
    pfc::string8 SkipSubmissionFormat;

    pfc::string8 SessionKey;

private:
    void get_data_raw(stream_writer* p_stream, abort_callback& p_abort) override;
    void set_data_raw(stream_reader* p_stream, t_size p_sizehint,
                      abort_callback& p_abort) override;

    static unsigned const Version = 2;
};

class NOVTABLE ScrobbleConfigNotify : public service_base
{
public:
    virtual void OnConfigChanged() = 0;

    static void NotifyChanged()
    {
        if (core_api::assert_main_thread()) {
            service_enum_t<ScrobbleConfigNotify> e;
            service_ptr_t<ScrobbleConfigNotify> ptr;
            while (e.next(ptr))
                ptr->OnConfigChanged();
        }
    }

    FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(ScrobbleConfigNotify)
};

extern ScrobbleConfig Config;

} // namespace foo_scrobble
