#ifndef ARIA_TRACK_H
#define ARIA_TRACK_H

#include "model/clip.h"
#include "model/routing.h"
#include "model/types.h"
#include <memory>
#include <string>
#include <vector>

namespace aria {

class AudioClip;  // forward decl for frozen clip

// ─── Clip placement on a track ──────────────────────────────────

/// A clip placed at a specific PPQN position on a track.
struct ClipPlacement {
    std::shared_ptr<Clip> clip;
    uint64_t start_ppqn = 0;  ///< Placement start (may differ from clip->start())
};

/// Track — base track model for all track types.
class Track {
public:
    explicit Track(TrackType type);
    virtual ~Track() = default;

    TrackID id() const;
    void set_name(const std::string& name);
    std::string name() const;
    TrackType type() const;

    void set_volume(double db);
    double volume() const;
    void set_pan(double pan);
    double pan() const;

    void set_muted(bool muted);
    bool is_muted() const;
    void set_soloed(bool soloed);
    bool is_soloed() const;

    // ─── Clip management ─────────────────────────────────────

    /// Add a clip at the given PPQN position.
    /// Returns true if the clip was added, false if the track is frozen.
    bool add_clip(std::shared_ptr<Clip> clip, uint64_t start_ppqn);

    /// Find the clip at a given PPQN position (returns nullptr if none).
    Clip* clip_at(uint64_t ppqn) const;

    /// Get all clip placements.
    const std::vector<ClipPlacement>& clips() const { return clips_; }

    // ─── Freeze state ────────────────────────────────────────

    /// Freeze the track (prevents clip edits).
    void set_frozen(bool frozen);

    /// Check if the track is frozen.
    bool is_frozen() const;

    /// Show or hide the frozen audio waveform preview.
    void set_show_frozen_audio(bool show);

    /// Check if frozen audio preview is visible.
    bool show_frozen_audio() const;

    /// Set the rendered frozen audio clip (nullptr to clear).
    void set_frozen_clip(std::shared_ptr<Clip> clip);

    /// Get the frozen audio clip (nullptr if not frozen or cleared).
    Clip* frozen_clip() const;

    /// Mark the frozen state as dirty (clip/FX edit detected).
    void set_freeze_dirty(bool dirty);

    /// Check if the frozen state is dirty.
    bool is_freeze_dirty() const;

    // ─── Routing (main output + sends) ────────────────────────

    /// Set the main routing output destination.
    void set_routing_out(const RouteTarget& target);

    /// Get the main routing output destination.
    const RouteTarget& routing_out() const;

    /// Add a send slot.
    void add_send(const TrackSendSlot& send);

    /// Get all send slots.
    const std::vector<TrackSendSlot>& sends() const;

    /// Clear all sends.
    void clear_sends();

private:
    TrackID id_;
    std::string name_;
    TrackType type_;
    double volume_db_ = 0.0;
    double pan_ = 0.0;
    bool muted_ = false;
    bool soloed_ = false;

    // ─── Clip placements ─────────────────────────────────────
    std::vector<ClipPlacement> clips_;

    // ─── Freeze state ────────────────────────────────────────
    bool frozen_ = false;
    bool show_frozen_ = false;
    bool freeze_dirty_ = false;
    std::shared_ptr<Clip> frozen_clip_;

    // ─── Routing fields ───────────────────────────────────────
    RouteTarget routing_out_;
    std::vector<TrackSendSlot> sends_;
};

} // namespace aria

#endif // ARIA_TRACK_H
