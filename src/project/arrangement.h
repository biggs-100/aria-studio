#ifndef ARIA_ARRANGEMENT_H
#define ARIA_ARRANGEMENT_H

#include "model/marker.h"
#include "model/types.h"
#include "timeline/tempo_track.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

namespace aria {

/// Arrangement — the timeline arrangement of tracks, clips, markers,
/// loop ranges, tempo, and time signatures.
///
/// Each project has exactly one Arrangement.
class Arrangement {
public:
    Arrangement() = default;
    ~Arrangement() = default;

    // ─── Track ordering ───────────────────────────────────────

    /// Get the display order of tracks.
    std::vector<TrackID> track_order() const;

    /// Get visible tracks — excludes children of folded groups.
    /// @param is_group_folded A predicate that returns true if a
    ///        TrackID belongs to a folded GroupTrack.
    std::vector<TrackID> visible_track_order(
        const std::function<bool(TrackID)>& is_child_hidden) const;

    /// Move a track to a new index in the display order.
    void move_track(TrackID id, uint32_t new_index);

    /// Insert a track at the given position in the track order.
    void insert_track(TrackID id, uint32_t index);

    /// Remove a track from the order (does NOT delete the track).
    void remove_track(TrackID id);

    // ─── Project length ───────────────────────────────────────

    void set_length(uint64_t ppqn);
    uint64_t length() const;

    // ─── Loop range ───────────────────────────────────────────

    void set_loop_range(uint64_t start, uint64_t end);
    std::pair<uint64_t, uint64_t> loop_range() const;
    void set_loop_enabled(bool enabled);
    bool loop_enabled() const;

    // ─── Markers ──────────────────────────────────────────────

    MarkerList& markers();
    const MarkerList& markers() const;

    // ─── Tempo ────────────────────────────────────────────────

    TempoTrack& tempo_track();
    const TempoTrack& tempo_track() const;

private:
    std::vector<TrackID> track_order_;
    uint64_t length_ppqn_ = 960 * 64;  // 16 bars at 4/4

    bool     loop_enabled_ = false;
    uint64_t loop_start_ppqn_ = 0;
    uint64_t loop_end_ppqn_   = 960 * 16;

    MarkerList markers_;
    TempoTrack tempo_track_;
};

} // namespace aria

#endif // ARIA_ARRANGEMENT_H
