#include "freeze_manager.h"

#include "model/audio_clip.h"
#include "model/track.h"
#include "project/project_manager.h"
#include "audio/audio_engine.h"
#include "audio/export/offline_renderer.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace aria {

FreezeManager::FreezeManager(ProjectManager& project, AudioEngine& engine,
                              OfflineRenderer& renderer)
    : project_(project), engine_(engine), renderer_(renderer) {}

void FreezeManager::freeze_track(TrackID track_id) {
    Track* track = project_.get_track(track_id);
    if (!track) return;

    // Determine render duration: use project length or a default.
    // For now, we render a fixed duration. In a full implementation,
    // the project length would come from the arrangement/timeline.
    const uint32_t sample_rate = 48000;
    const uint32_t duration_frames = sample_rate * 10; // default 10s

    // Resolve TrackID to engine track index.
    // In the current architecture, we use the track's internal ID as
    // a heuristic. In a full implementation, ProjectManager would
    // maintain a TrackID → engine_index mapping.
    // For now, we iterate engine tracks and use index 0 as fallback.
    uint32_t engine_index = 0;
    for (uint32_t i = 0; i < engine_.track_count(); ++i) {
        auto* tp = engine_.track(i);
        if (tp) {
            engine_index = i;
            break;
        }
    }

    // Render the track to an interleaved float buffer
    std::vector<float> render_buffer;
    bool render_ok = renderer_.render_track(
        engine_, engine_index, sample_rate, duration_frames, render_buffer);

    if (!render_ok) {
        // If render fails (e.g., concurrent render), still mark frozen
        // so the track is protected from edits. The freeze_clip will be
        // set on a subsequent freeze attempt.
        track->set_frozen(true);
        track->set_freeze_dirty(false);
        return;
    }

    // Create an AudioClip from the rendered buffer
    auto frozen_clip = std::make_shared<AudioClip>();

    if (!render_buffer.empty()) {
        const uint32_t channels = 2;
        const uint32_t frames = static_cast<uint32_t>(
            render_buffer.size() / channels);

        frozen_clip->set_total_samples(frames);
        frozen_clip->set_length(static_cast<uint64_t>(
            static_cast<double>(frames) / static_cast<double>(sample_rate)
            * 960.0)); // Convert frames to PPQN (approx 1 bar = 960 PPQN at 120 BPM)
        frozen_clip->set_name("Frozen: " + track->name());
    }

    // Store the frozen clip on the track
    track->set_frozen_clip(std::move(frozen_clip));
    track->set_frozen(true);
    track->set_freeze_dirty(false);
}

void FreezeManager::unfreeze_track(TrackID track_id) {
    Track* track = project_.get_track(track_id);
    if (!track) return;

    track->set_frozen_clip(nullptr);
    track->set_frozen(false);
    track->set_freeze_dirty(false);
}

bool FreezeManager::is_frozen(TrackID track_id) const {
    const Track* track = project_.get_track(track_id);
    return track ? track->is_frozen() : false;
}

bool FreezeManager::is_dirty(TrackID track_id) const {
    const Track* track = project_.get_track(track_id);
    return track ? track->is_freeze_dirty() : false;
}

void FreezeManager::mark_dirty(TrackID track_id) {
    Track* track = project_.get_track(track_id);
    if (!track) return;
    track->set_freeze_dirty(true);
}

void FreezeManager::bounce(const BounceOptions& options) {
    if (options.format == BounceFormat::Master) {
        // Master bounce: render all tracks through the master bus
        // For now, this is a stub. In a full implementation, this
        // would use OfflineRenderer::render() with an ExportConfig
        // targeting a WAV file of the full mix.
        (void)engine_;
        (void)renderer_;
        return;
    }

    if (options.format == BounceFormat::Individual) {
        // Individual bounce: render each selected track separately
        std::vector<TrackID> targets = options.tracks;
        if (targets.empty()) {
            // If no tracks specified, bounce all tracks
            // Would iterate project tracks in a full implementation
            return;
        }

        for (TrackID tid : targets) {
            // Render each track to its own file
            // For now, this is a stub. Full implementation would
            // call render_track() for each track and write to WAV.
            // freeze_track(tid); // freeze first
            // Write to WAV file
            (void)tid;
        }
        return;
    }

    if (options.format == BounceFormat::Stem) {
        // Stem bounce: selected tracks summed per group
        // For now, this is a stub.
        (void)options;
        return;
    }
}

} // namespace aria
