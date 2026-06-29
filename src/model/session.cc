#include "session.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════

Scene* Session::find_scene(SceneID id) {
    return const_cast<Scene*>(std::as_const(*this).find_scene(id));
}

const Scene* Session::find_scene(SceneID id) const {
    for (const auto& s : scenes_) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

ClipSlot* Session::find_slot(TrackID track, SceneID scene) {
    return const_cast<ClipSlot*>(std::as_const(*this).find_slot(track, scene));
}

const ClipSlot* Session::find_slot(TrackID track, SceneID scene) const {
    auto track_it = grid_.find(track);
    if (track_it == grid_.end()) return nullptr;
    auto scene_it = track_it->second.find(scene);
    if (scene_it == track_it->second.end()) return nullptr;
    return &scene_it->second;
}

// ═══════════════════════════════════════════════════════════════
// Scene management
// ═══════════════════════════════════════════════════════════════

SceneID Session::add_scene(const std::string& name) {
    SceneID id{next_scene_id_++};
    Scene scene;
    scene.id = id;
    scene.name = name;
    scene.order_index = static_cast<uint32_t>(scenes_.size());
    scenes_.push_back(std::move(scene));
    return id;
}

void Session::remove_scene(SceneID id) {
    scenes_.erase(
        std::remove_if(scenes_.begin(), scenes_.end(),
                       [id](const Scene& s) { return s.id == id; }),
        scenes_.end());

    // Remove all slots for this scene from the grid
    for (auto& [track_id, scene_map] : grid_) {
        scene_map.erase(id);
    }

    // Clear any playing clip referencing this scene
    for (auto it = playing_clips_.begin(); it != playing_clips_.end(); ) {
        if (it->second == id) {
            it = playing_clips_.erase(it);
        } else {
            ++it;
        }
    }
}

void Session::set_scene_name(SceneID id, const std::string& name) {
    auto* scene = find_scene(id);
    if (scene) scene->name = name;
}

std::string Session::scene_name(SceneID id) const {
    const auto* scene = find_scene(id);
    return scene ? scene->name : "";
}

// ═══════════════════════════════════════════════════════════════
// Clip grid
// ═══════════════════════════════════════════════════════════════

void Session::set_clip_slot(TrackID track, SceneID scene,
                              std::shared_ptr<Clip> clip) {
    auto& slot = grid_[track][scene];
    slot.clip = std::move(clip);
    // Reset state when a new clip is placed
    slot.state = LaunchState::Stopped;
    slot.follow_action = FollowAction::None;
}

Clip* Session::clip_at(TrackID track, SceneID scene) const {
    const auto* slot = find_slot(track, scene);
    return slot ? slot->clip.get() : nullptr;
}

LaunchState Session::slot_state(TrackID track, SceneID scene) const {
    const auto* slot = find_slot(track, scene);
    return slot ? slot->state : LaunchState::Stopped;
}

// ═══════════════════════════════════════════════════════════════
// Launch
// ═══════════════════════════════════════════════════════════════

void Session::launch_clip(TrackID track, SceneID scene) {
    auto* slot = find_slot(track, scene);
    if (!slot || !slot->clip) return;

    // Stop the previously playing clip on this track
    auto prev_it = playing_clips_.find(track);
    if (prev_it != playing_clips_.end()) {
        auto* prev_slot = find_slot(track, prev_it->second);
        if (prev_slot) {
            prev_slot->state = LaunchState::Stopped;
        }
    }

    // Start the new clip
    slot->state = LaunchState::Playing;
    playing_clips_[track] = scene;
}

void Session::launch_scene(SceneID scene) {
    for (auto& [track_id, scene_map] : grid_) {
        auto it = scene_map.find(scene);
        if (it == scene_map.end()) continue;
        if (!it->second.clip) continue;

        // Stop previous clip on this track
        auto prev_it = playing_clips_.find(track_id);
        if (prev_it != playing_clips_.end()) {
            auto* prev_slot = find_slot(track_id, prev_it->second);
            if (prev_slot) {
                prev_slot->state = LaunchState::Stopped;
            }
        }

        // Launch this scene's clip
        it->second.state = LaunchState::Playing;
        playing_clips_[track_id] = scene;
    }
}

void Session::stop_clip(TrackID track) {
    auto it = playing_clips_.find(track);
    if (it == playing_clips_.end()) return;

    auto* slot = find_slot(track, it->second);
    if (slot) {
        slot->state = LaunchState::Stopped;
    }
    playing_clips_.erase(it);
}

Clip* Session::playing_clip(TrackID track) const {
    auto it = playing_clips_.find(track);
    if (it == playing_clips_.end()) return nullptr;

    const auto* slot = find_slot(track, it->second);
    return slot ? slot->clip.get() : nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Follow actions
// ═══════════════════════════════════════════════════════════════

void Session::set_scene_follow_action(SceneID id, FollowAction action) {
    auto* scene = find_scene(id);
    if (scene) scene->follow_action = action;
}

FollowAction Session::scene_follow_action(SceneID id) const {
    const auto* scene = find_scene(id);
    return scene ? scene->follow_action : FollowAction::None;
}

void Session::set_slot_follow_action(TrackID track, SceneID scene,
                                      FollowAction action) {
    auto* slot = find_slot(track, scene);
    if (slot) slot->follow_action = action;
}

FollowAction Session::slot_follow_action(TrackID track, SceneID scene) const {
    const auto* slot = find_slot(track, scene);
    return slot ? slot->follow_action : FollowAction::None;
}

bool Session::evaluate_follow_actions(TrackID track) {
    auto it = playing_clips_.find(track);
    if (it == playing_clips_.end()) return false;

    SceneID current_scene = it->second;

    // Determine the follow action (slot-level takes priority, fall back to scene-level)
    FollowAction action = FollowAction::None;
    const auto* slot = find_slot(track, current_scene);
    if (slot && slot->follow_action != FollowAction::None) {
        action = slot->follow_action;
    } else {
        const auto* scene = find_scene(current_scene);
        if (scene) action = scene->follow_action;
    }

    switch (action) {
        case FollowAction::None:
            // Default: stop the clip
            stop_clip(track);
            return false;

        case FollowAction::Stop:
            stop_clip(track);
            return false;

        case FollowAction::PlayNext: {
            // Find the index of the current scene
            int current_idx = -1;
            for (size_t i = 0; i < scenes_.size(); ++i) {
                if (scenes_[i].id == current_scene) {
                    current_idx = static_cast<int>(i);
                    break;
                }
            }
            // If there is a next scene, launch it
            if (current_idx >= 0 && current_idx + 1 < static_cast<int>(scenes_.size())) {
                SceneID next_scene = scenes_[current_idx + 1].id;
                // Find what clip to play on this track in the next scene
                auto* next_slot = find_slot(track, next_scene);
                if (next_slot && next_slot->clip) {
                    launch_clip(track, next_scene);
                    return true;
                }
            }
            // No next scene or no clip — stop
            stop_clip(track);
            return false;
        }

        case FollowAction::PlayRandom: {
            // Collect all scenes that have a clip on this track
            std::vector<SceneID> candidates;
            auto track_it = grid_.find(track);
            if (track_it != grid_.end()) {
                for (const auto& [sid, slot] : track_it->second) {
                    if (slot.clip) candidates.push_back(sid);
                }
            }
            // Pick a random one (simple: use the first different one)
            // In a real implementation this would use a proper RNG
            if (!candidates.empty()) {
                SceneID random_scene = candidates[0];
                if (random_scene == current_scene && candidates.size() > 1) {
                    random_scene = candidates[1];
                }
                launch_clip(track, random_scene);
                return true;
            }
            stop_clip(track);
            return false;
        }

        case FollowAction::ContinueAsLoop: {
            // Re-launch the same clip
            auto* current_slot = find_slot(track, current_scene);
            if (current_slot && current_slot->clip) {
                launch_clip(track, current_scene);
                return true;
            }
            stop_clip(track);
            return false;
        }
    }

    return false;
}

// ═══════════════════════════════════════════════════════════════
// Crossfader
// ═══════════════════════════════════════════════════════════════

void Session::set_crossfader_position(double pos) {
    crossfader_.position = std::clamp(pos, -1.0, 1.0);
}

void Session::set_crossfader_curve(CrossfaderCurve curve) {
    crossfader_.curve = curve;
}

void Session::set_track_assignment(TrackID track,
                                    CrossfaderAssignment assignment) {
    track_assignments_[track] = assignment;
}

CrossfaderAssignment Session::track_assignment(TrackID track) const {
    auto it = track_assignments_.find(track);
    if (it != track_assignments_.end()) return it->second;
    return CrossfaderAssignment::None;
}

double Session::evaluate_crossfader_gain(TrackID track) const {
    auto assignment = track_assignment(track);

    switch (assignment) {
        case CrossfaderAssignment::None:
        case CrossfaderAssignment::Both:
            return 1.0;

        case CrossfaderAssignment::A: {
            // Full gain at position -1.0..0.0, fades to 0 at +1.0
            double pos = crossfader_.position;
            if (pos <= 0.0) return 1.0;
            double gain;
            if (crossfader_.curve == CrossfaderCurve::EqualPower) {
                gain = std::cos(pos * std::numbers::pi_v<double> / 2.0);
            } else {
                gain = 1.0 - pos;
            }
            return (gain < 1e-15) ? 0.0 : gain;
        }

        case CrossfaderAssignment::B: {
            // Full gain at position 0.0..+1.0, fades to 0 at -1.0
            double pos = crossfader_.position;
            if (pos >= 0.0) return 1.0;
            double gain;
            if (crossfader_.curve == CrossfaderCurve::EqualPower) {
                gain = std::cos(-pos * std::numbers::pi_v<double> / 2.0);
            } else {
                gain = 1.0 + pos;
            }
            return (gain < 1e-15) ? 0.0 : gain;
        }
    }

    return 1.0;
}

// ═══════════════════════════════════════════════════════════════
// Quantization
// ═══════════════════════════════════════════════════════════════

void Session::set_launch_quantization(LaunchQuantization q) {
    launch_quantization_ = q;
}

} // namespace aria
