#include "ScrobbleConfig.h"
#include "WebService.h"
#include "fb2ksdk.h"

#include "ScrobbleService.h"
#include "ServiceHelper.h"
#include "Track.h"

namespace foo_scrobble
{

namespace
{

using SecondsD = std::chrono::duration<double>;

std::chrono::duration<double> const MinRequiredTrackLength{30.0};
std::chrono::duration<double> const MaxElapsedPlaytime{240.0};
std::chrono::duration<double> const NowPlayingMinimumPlaybackTime{3.0};

class TitleformatContext
{
public:
    titleformat_object::ptr const& GetArtistFormat() const { return artistFormat_; }

    titleformat_object::ptr const& GetTitleFormat() const { return titleFormat_; }

    titleformat_object::ptr const& GetAlbumArtistFormat() const
    {
        return albumArtistFormat_;
    }

    titleformat_object::ptr const& GetAlbumFormat() const { return albumFormat_; }

    titleformat_object::ptr const& GetTrackNumberFormat() const
    {
        return trackNumberFormat_;
    }

    titleformat_object::ptr const& GetMusicBrainzTrackIdFormat() const
    {
        return mbidFormat_;
    }

    titleformat_object::ptr const& GetSkipSubmissionFormat() const
    {
        return skipSubmissionFormat_;
    }

    void Recompile(ScrobbleConfig const& config)
    {
        auto compiler = titleformat_compiler::get();

        Compile(compiler, artistFormat_, config.ArtistMapping, DefaultArtistMapping);
        Compile(compiler, titleFormat_, config.TitleMapping, DefaultTitleMapping);
        Compile(compiler, albumArtistFormat_, config.AlbumArtistMapping,
                DefaultAlbumArtistMapping);
        Compile(compiler, albumFormat_, config.AlbumMapping, DefaultAlbumMapping);
        Compile(compiler, trackNumberFormat_, config.TrackNumberMapping,
                DefaultTrackNumberMapping);
        Compile(compiler, mbidFormat_, config.MBTrackIdMapping, DefaultMBTrackIdMapping);

        if (!config.SkipSubmissionFormat.is_empty())
            compiler->compile(skipSubmissionFormat_, config.SkipSubmissionFormat);
        else
            skipSubmissionFormat_ = nullptr;
    }

private:
    static bool Compile(titleformat_compiler::ptr& compiler,
                        titleformat_object::ptr& format, char const* spec,
                        char const* fallbackSpec)
    {
        return compiler->compile(format, spec) || compiler->compile(format, fallbackSpec);
    }

    titleformat_object::ptr artistFormat_;
    titleformat_object::ptr titleFormat_;
    titleformat_object::ptr albumArtistFormat_;
    titleformat_object::ptr albumFormat_;
    titleformat_object::ptr trackNumberFormat_;
    titleformat_object::ptr mbidFormat_;
    titleformat_object::ptr skipSubmissionFormat_;
};

class PendingTrack : public Track
{
public:
    std::chrono::duration<double> RequiredScrobbleTime() const
    {
        if (Duration <= SecondsD::zero())
            return MinRequiredTrackLength;
#ifdef _DEBUG
        return std::min(Duration, SecondsD{2.0});
#else
        return std::min(Duration * 0.5, MaxElapsedPlaytime);
#endif
    }

    bool HasRequiredFields() const
    {
        return Artist.get_length() > 0 && Title.get_length() > 0;
    }

    bool IsSkipped() const { return skip_; }

    bool CanScrobble(SecondsD const& playbackTime, bool logFailure = false) const
    {
        if (playbackTime < RequiredScrobbleTime())
            return false;

        if (IsSkipped()) {
            if (logFailure)
                FB2K_console_formatter()
                    << "foo_scrobble: Skipping track based on skip conditions";
            return false;
        }

        if (!HasRequiredFields()) {
            if (logFailure)
                FB2K_console_formatter()
                    << "foo_scrobble: Skipping track due to missing artist or title";
            return false;
        }

        return true;
    }

    bool ShouldSendNowPlaying(std::chrono::duration<double> elapsedPlayback) const
    {
        return !notifiedNowPlaying_ && !IsSkipped() && HasRequiredFields() &&
               elapsedPlayback >= NowPlayingMinimumPlaybackTime;
    }

    void NowPlayingSent() { notifiedNowPlaying_ = true; }

    void Format(metadb_handle& track, TitleformatContext& formatContext)
    {
        Timestamp = unix_clock::now();
        Reformat(track, formatContext);
    }

    void Reformat(metadb_handle& track, TitleformatContext& formatContext)
    {
        track.format_title(nullptr, Artist, formatContext.GetArtistFormat(), nullptr);
        track.format_title(nullptr, Title, formatContext.GetTitleFormat(), nullptr);
        track.format_title(nullptr, AlbumArtist, formatContext.GetAlbumArtistFormat(),
                           nullptr);
        track.format_title(nullptr, Album, formatContext.GetAlbumFormat(), nullptr);
        track.format_title(nullptr, MusicBrainzId,
                           formatContext.GetMusicBrainzTrackIdFormat(), nullptr);
        track.format_title(nullptr, TrackNumber, formatContext.GetTrackNumberFormat(),
                           nullptr);

        auto skipFormat = formatContext.GetSkipSubmissionFormat();
        if (!skipFormat.is_empty()) {
            pfc::string8_fast skip;
            track.format_title(nullptr, skip, formatContext.GetSkipSubmissionFormat(),
                               nullptr);
            skip_ = !skip.is_empty();
        }

        Duration = SecondsD{track.get_length()};
        IsDynamic = false;

        lastUpdateTime_ = unix_clock::now();
    }

    void Format(file_info const& dynamicTrack, TitleformatContext& formatContext)
    {
        auto pc = static_api_ptr_t<playback_control>();
        pc->playback_format_title(nullptr, Artist, formatContext.GetArtistFormat(),
                                  nullptr, playback_control::display_level_titles);
        pc->playback_format_title(nullptr, Title, formatContext.GetTitleFormat(), nullptr,
                                  playback_control::display_level_titles);
        pc->playback_format_title(nullptr, Album, formatContext.GetAlbumFormat(), nullptr,
                                  playback_control::display_level_titles);
        AlbumArtist.force_reset();
        TrackNumber.force_reset();
        MusicBrainzId.force_reset();

        auto skipFormat = formatContext.GetSkipSubmissionFormat();
        if (!skipFormat.is_empty()) {
            pfc::string8_fast skip;
            pc->playback_format_title(nullptr, skip,
                                      formatContext.GetSkipSubmissionFormat(), nullptr,
                                      playback_control::display_level_titles);
            skip_ = !skip.is_empty();
        }

        Timestamp = unix_clock::now();
        Duration = SecondsD{dynamicTrack.get_length()};
        IsDynamic = true;

        lastUpdateTime_ = unix_clock::now();
    }

private:
    unix_clock::time_point lastUpdateTime_;
    bool notifiedNowPlaying_ = false;
    bool skip_ = false;
};
} // namespace

class PlaybackScrobbler
    : public service_multi_inherit<play_callback_static, ScrobbleConfigNotify>
{
public:
    PlaybackScrobbler() = default;
    virtual ~PlaybackScrobbler() = default;

    // #pragma region play_callback
    virtual void on_playback_starting(play_control::t_track_command p_command,
                                      bool p_paused) override;
    virtual void on_playback_new_track(metadb_handle_ptr p_track) override;
    virtual void on_playback_stop(play_control::t_stop_reason p_reason) override;
    virtual void on_playback_seek(double p_time) override;
    virtual void on_playback_pause(bool p_state) override;
    virtual void on_playback_edited(metadb_handle_ptr p_track) override;
    virtual void on_playback_dynamic_info(file_info const& p_info) override;
    virtual void on_playback_dynamic_info_track(file_info const& p_info) override;
    virtual void on_playback_time(double p_time) override;
    virtual void on_volume_change(float p_new_val) override;
    // #pragma endregion play_callback

    // #pragma region play_callback_static
    virtual unsigned get_flags() override;
    // #pragma endregion play_callback_static

    // #pragma region ScrobbleConfigNotify
    virtual void OnConfigChanged() override
    {
        if (formatContext_)
            formatContext_->Recompile(Config);
    }
    // #pragma endregion ScrobbleConfigNotify

private:
    bool ShouldScrobble(metadb_handle_ptr const& track) const;
    void FlushCurrentTrack();

    ScrobbleService& GetScrobbleService()
    {
        if (scrobbler_ == nullptr)
            scrobbler_ = standard_api_create_t<ScrobbleService>().get_ptr();

        return *scrobbler_;
    }

    TitleformatContext& GetFormatContext()
    {
        if (!formatContext_) {
            formatContext_ = std::make_unique<TitleformatContext>();
            formatContext_->Recompile(Config);
        }

        return *formatContext_;
    }

    ScrobbleConfig const& config_{Config};
    PendingTrack pendingTrack_;
    SecondsD accumulatedPlaybackTime_{};
    SecondsD lastPlaybackTime_{};
    bool isActive_ = false;

    std::unique_ptr<TitleformatContext> formatContext_;
    ScrobbleService* scrobbler_ = nullptr;
};

#pragma region play_callback_static
void PlaybackScrobbler::on_playback_starting(play_control::t_track_command /*p_command*/,
                                             bool /*p_paused*/)
{
    // nothing
}

void PlaybackScrobbler::on_playback_new_track(metadb_handle_ptr p_track)
{
    FlushCurrentTrack();

    isActive_ = config_.EnableScrobbling;
    if (!isActive_)
        return;

    if (p_track->get_length() <= 0)
        return;

    if (ShouldScrobble(p_track)) {
        pendingTrack_.Format(*p_track, GetFormatContext());
    } else {
        isActive_ = false;
        FB2K_console_formatter() << "foo_scrobble: Skipping track not in media library.";
    }
}

void PlaybackScrobbler::on_playback_stop(play_control::t_stop_reason p_reason)
{
    if (p_reason == playback_control::stop_reason_shutting_down)
        GetScrobbleService().Shutdown();
    FlushCurrentTrack();
}

void PlaybackScrobbler::on_playback_seek(double p_time)
{
    if (!isActive_)
        return;

    lastPlaybackTime_ = SecondsD(p_time);
}

void PlaybackScrobbler::on_playback_pause(bool /*p_state*/) {}

void PlaybackScrobbler::on_playback_edited(metadb_handle_ptr p_track)
{
    if (!isActive_)
        return;

    if (!pendingTrack_.IsDynamic)
        pendingTrack_.Reformat(*p_track, GetFormatContext());
}

void PlaybackScrobbler::on_playback_dynamic_info(file_info const& /*p_info*/) {}

void PlaybackScrobbler::on_playback_dynamic_info_track(file_info const& p_info)
{
    FlushCurrentTrack();

    isActive_ = config_.EnableScrobbling && config_.SubmitDynamicSources;
    if (!isActive_)
        return;

    pendingTrack_.Format(p_info, GetFormatContext());
}

void PlaybackScrobbler::on_playback_time(double p_time)
{
    if (!isActive_)
        return;

    accumulatedPlaybackTime_ += SecondsD(p_time) - lastPlaybackTime_;
    lastPlaybackTime_ = SecondsD(p_time);

    if (pendingTrack_.ShouldSendNowPlaying(accumulatedPlaybackTime_)) {
        GetScrobbleService().SendNowPlayingAsync(pendingTrack_);
        pendingTrack_.NowPlayingSent();
    }
}

void PlaybackScrobbler::on_volume_change(float /*p_new_val*/) {}

unsigned int PlaybackScrobbler::get_flags()
{
    return flag_on_playback_time | flag_on_playback_dynamic_info_track |
           flag_on_playback_edited | flag_on_playback_seek | flag_on_playback_stop |
           flag_on_playback_new_track;
}

#pragma endregion play_callback_static

bool PlaybackScrobbler::ShouldScrobble(metadb_handle_ptr const& track) const
{
    return !config_.SubmitOnlyInLibrary ||
           library_manager::get()->is_item_in_library(track);
}

void PlaybackScrobbler::FlushCurrentTrack()
{
    if (isActive_ && pendingTrack_.CanScrobble(accumulatedPlaybackTime_, true))
        GetScrobbleService().ScrobbleAsync(std::move(static_cast<Track&>(pendingTrack_)));

    accumulatedPlaybackTime_ = SecondsD::zero();
    lastPlaybackTime_ = SecondsD::zero();
    isActive_ = false;
    pendingTrack_ = PendingTrack();
}

static service_factory_single_v_t<PlaybackScrobbler, play_callback_static,
                                  ScrobbleConfigNotify>
    g_PlaybackScrobblerFactory;

} // namespace foo_scrobble
