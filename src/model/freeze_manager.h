#ifndef ARIA_FREEZE_MANAGER_H
#define ARIA_FREEZE_MANAGER_H

#include "model/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria {

class ProjectManager;
class AudioEngine;
class OfflineRenderer;

// ─── BounceFormat ───────────────────────────────────────────────

/// Format for bounce export.
enum class BounceFormat : uint8_t {
    Individual,  ///< One file per track (e.g., T1.wav, T2.wav).
    Stem,        ///< Selected tracks summed per group.
    Master       ///< Full mix rendered to a single stereo file.
};

// ─── BounceOptions ──────────────────────────────────────────────

/// Configuration for a bounce operation.
struct BounceOptions {
    std::vector<TrackID> tracks;          ///< Tracks to bounce (empty = all).
    BounceFormat format = BounceFormat::Master;
    uint64_t     range_start = 0;         ///< Start PPQN.
    uint64_t     range_end = 0;           ///< End PPQN (0 = entire project).
    std::string  output_directory;        ///< Output directory for files.
    uint32_t     sample_rate = 48000;     ///< Output sample rate.
    uint16_t     bit_depth = 24;          ///< Output bit depth.
};

// ─── FreezeManager ──────────────────────────────────────────────

/// Orchestrates freeze/unfreeze lifecycle and bounce operations.
///
/// Freeze: renders a track's signal through its FX chain to an
/// AudioClip, stores the clip as the track's frozen_clip, bypasses
/// all FX, and marks the track as frozen.
///
/// Unfreeze: removes the frozen_clip, re-enables FX, marks unfrozen.
///
/// Dirty flag: when a frozen track's clips or FX change, the
/// freeze render is stale until re-freeze.
///
/// Bounce: renders one or more tracks to WAV files in Individual,
/// Stem, or Master mode.
///
/// Thread safety:
///   - All methods are intended for the control thread.
///   - freeze_track() and bounce() are blocking (they call the
///     OfflineRenderer which processes the audio graph).
class FreezeManager {
public:
    /// Construct a FreezeManager.
    /// @param project  ProjectManager (for track access).
    /// @param engine   AudioEngine (for render).
    /// @param renderer OfflineRenderer (for per-track render).
    FreezeManager(ProjectManager& project, AudioEngine& engine,
                  OfflineRenderer& renderer);

    /// Freeze a track: render its signal to an AudioClip and
    /// store it on the track. Marks the track as frozen.
    /// If the track is already frozen, re-freezes it.
    void freeze_track(TrackID track);

    /// Unfreeze a track: remove the frozen clip, re-enable FX,
    /// and mark the track as not frozen.
    /// Harmless if the track is not frozen.
    void unfreeze_track(TrackID track);

    /// Check if a track is frozen.
    bool is_frozen(TrackID track) const;

    /// Check if a frozen track's freeze state is dirty (stale).
    bool is_dirty(TrackID track) const;

    /// Mark a track's freeze state as dirty.
    /// Should be called when clips or FX change on a frozen track.
    void mark_dirty(TrackID track);

    /// Bounce tracks to WAV files.
    /// Blocks until complete.
    /// @param options  Bounce configuration.
    void bounce(const BounceOptions& options);

private:
    ProjectManager&    project_;
    AudioEngine&       engine_;
    OfflineRenderer&   renderer_;
};

} // namespace aria

#endif // ARIA_FREEZE_MANAGER_H
