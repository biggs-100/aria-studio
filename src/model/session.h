#ifndef ARIA_SESSION_H
#define ARIA_SESSION_H

#include "model/clip.h"
#include "model/types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aria {

// ─── LaunchState ────────────────────────────────────────────────

/// The state of a clip slot in the session grid.
enum class LaunchState : uint8_t {
    Stopped,
    Triggered,
    Playing
};

// ─── FollowAction ──────────────────────────────────────────────

/// What happens when a scene or clip finishes playback.
enum class FollowAction : uint8_t {
    None,           ///< Do nothing — clip stops.
    Stop,           ///< Stop playback.
    PlayNext,       ///< Trigger the next scene in order.
    PlayRandom,     ///< Trigger a random scene.
    ContinueAsLoop  ///< Restart the clip from the beginning.
};

// ─── Scene ──────────────────────────────────────────────────────

/// A session scene — a row in the clip grid.
struct Scene {
    SceneID     id;
    std::string name;
    uint32_t    color       = 0xFF888888;  ///< ARGB color.
    uint32_t    order_index = 0;
    FollowAction follow_action = FollowAction::None;
};

// ─── ClipSlot ───────────────────────────────────────────────────

/// A single cell in the session grid (track × scene).
struct ClipSlot {
    std::shared_ptr<Clip> clip;
    LaunchState   state         = LaunchState::Stopped;
    FollowAction  follow_action = FollowAction::None;
};

// ─── CrossfaderAssignment ──────────────────────────────────────

/// Which side of the crossfader a track is assigned to.
enum class CrossfaderAssignment : uint8_t {
    None,  ///< Not affected by crossfader (always full gain).
    A,     ///< Left side — gain decreases as crossfader moves right.
    B,     ///< Right side — gain decreases as crossfader moves left.
    Both   ///< Both sides — always full gain.
};

// ─── CrossfaderCurve ───────────────────────────────────────────

/// The curve type for crossfader gain interpolation.
enum class CrossfaderCurve : uint8_t {
    Linear,
    EqualPower
};

// ─── LaunchQuantization ────────────────────────────────────────

/// Quantization level for clip/scene launches.
enum class LaunchQuantization : uint8_t {
    None,
    Bar,
    Half,
    Quarter,
    Eighth
};

// ─── Crossfader ────────────────────────────────────────────────

/// Global crossfader for A/B track mixing.
struct Crossfader {
    double          position = 0.0;  ///< -1.0 (full A) to +1.0 (full B).
    CrossfaderCurve curve    = CrossfaderCurve::EqualPower;
};

// ─── Session ───────────────────────────────────────────────────

/// The session view model — a grid of clip slots organized by tracks
/// (columns) and scenes (rows).
///
/// Supports clip launching, scene triggering, follow actions,
/// crossfader assignment, and quantization.
///
/// Owned by ProjectData, parallel to Arrangement.
class Session {
public:
    Session() = default;
    ~Session() = default;

    // ─── Scene management ─────────────────────────────────────

    /// Add a new scene with the given name. Returns the assigned SceneID.
    SceneID add_scene(const std::string& name);

    /// Remove a scene and all its clip slots.
    void remove_scene(SceneID scene);

    /// Set the name of a scene.
    void set_scene_name(SceneID scene, const std::string& name);

    /// Get the name of a scene (empty string if not found).
    std::string scene_name(SceneID scene) const;

    /// Get all scenes in order.
    const std::vector<Scene>& scenes() const { return scenes_; }

    /// Get the number of scenes.
    size_t scene_count() const { return scenes_.size(); }

    // ─── Clip grid ────────────────────────────────────────────

    /// Place a clip in a slot at the given track × scene.
    void set_clip_slot(TrackID track, SceneID scene,
                       std::shared_ptr<Clip> clip);

    /// Get the clip in a slot (nullptr if empty).
    Clip* clip_at(TrackID track, SceneID scene) const;

    /// Get the launch state of a slot.
    LaunchState slot_state(TrackID track, SceneID scene) const;

    // ─── Launch ───────────────────────────────────────────────

    /// Launch a single clip on a track, stopping any previously
    /// playing clip on that track.
    void launch_clip(TrackID track, SceneID scene);

    /// Launch all clips in a scene simultaneously.
    void launch_scene(SceneID scene);

    /// Stop the playing clip on a track.
    void stop_clip(TrackID track);

    /// Get the currently playing clip on a track (nullptr if none).
    Clip* playing_clip(TrackID track) const;

    // ─── Follow actions ───────────────────────────────────────

    /// Set the per-scene follow action.
    void set_scene_follow_action(SceneID scene, FollowAction action);

    /// Get the per-scene follow action.
    FollowAction scene_follow_action(SceneID scene) const;

    /// Set the per-slot follow action.
    void set_slot_follow_action(TrackID track, SceneID scene,
                                FollowAction action);

    /// Get the per-slot follow action.
    FollowAction slot_follow_action(TrackID track, SceneID scene) const;

    /// Evaluate follow actions when a clip finishes playback.
    /// Returns true if a follow action triggered a new clip/transition.
    /// The track parameter identifies which track's clip finished.
    bool evaluate_follow_actions(TrackID track);

    // ─── Crossfader ──────────────────────────────────────────

    /// Access the global crossfader.
    Crossfader& crossfader() { return crossfader_; }
    const Crossfader& crossfader() const { return crossfader_; }

    /// Set the crossfader position (-1.0 to +1.0).
    void set_crossfader_position(double pos);

    /// Get the crossfader position.
    double crossfader_position() const { return crossfader_.position; }

    /// Set the crossfader curve type.
    void set_crossfader_curve(CrossfaderCurve curve);

    /// Get the crossfader curve type.
    CrossfaderCurve crossfader_curve() const { return crossfader_.curve; }

    /// Assign a track to a crossfader side.
    void set_track_assignment(TrackID track, CrossfaderAssignment assignment);

    /// Get the crossfader assignment for a track.
    CrossfaderAssignment track_assignment(TrackID track) const;

    /// Compute the effective crossfader gain for a track based on its
    /// assignment and the current crossfader position.
    double evaluate_crossfader_gain(TrackID track) const;

    // ─── Quantization ────────────────────────────────────────

    /// Set the launch quantization level.
    void set_launch_quantization(LaunchQuantization q);

    /// Get the launch quantization level.
    LaunchQuantization launch_quantization() const {
        return launch_quantization_;
    }

private:
    std::vector<Scene> scenes_;
    std::unordered_map<TrackID,
        std::unordered_map<SceneID, ClipSlot>> grid_;
    std::unordered_map<TrackID, SceneID> playing_clips_;
    std::unordered_map<TrackID, CrossfaderAssignment> track_assignments_;
    Crossfader crossfader_;
    LaunchQuantization launch_quantization_ = LaunchQuantization::None;
    uint32_t next_scene_id_ = 1;

    /// Find a scene by ID (nullptr if not found).
    Scene* find_scene(SceneID id);
    const Scene* find_scene(SceneID id) const;

    /// Find a clip slot (nullptr if not found).
    ClipSlot* find_slot(TrackID track, SceneID scene);
    const ClipSlot* find_slot(TrackID track, SceneID scene) const;
};

} // namespace aria

#endif // ARIA_SESSION_H
