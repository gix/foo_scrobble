#pragma once
#include "Track.h"
#include "fb2ksdk.h"

#include <vector>

namespace foo_scrobble
{

class ScrobbleCache : public cfg_var
{
public:
    static ScrobbleCache& Get() { return instance_; }

    bool IsEmpty() const { return tracks_.empty(); }
    size_t Count() const { return tracks_.size(); }
    Track& operator[](size_t index) { return tracks_[index]; }

    void Add(Track track) { tracks_.push_back(std::move(track)); }

    void Evict(size_t count)
    {
        count = std::min(count, tracks_.size());
        tracks_.erase(tracks_.begin(), tracks_.begin() + count);
    }

private:
    ScrobbleCache(GUID const& guid)
        : cfg_var(guid)
    {
    }

    virtual void get_data_raw(stream_writer* stream, abort_callback& abort) override;
    virtual void set_data_raw(stream_reader* stream, t_size sizehint,
                              abort_callback& abort) override;

    std::vector<Track> tracks_;
    static ScrobbleCache instance_;
};

} // namespace foo_scrobble
