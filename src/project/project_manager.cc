#include "project_manager.h"

#include <algorithm>
#include <utility>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════

auto ProjectManager::find_project(ProjectID id) -> ProjectData* {
    auto it = projects_.find(id);
    return (it != projects_.end()) ? &it->second : nullptr;
}

auto ProjectManager::find_project(ProjectID id) const -> const ProjectData* {
    auto it = projects_.find(id);
    return (it != projects_.end()) ? &it->second : nullptr;
}

auto ProjectManager::find_track(TrackID id) -> Track* {
    for (auto& [pid, pd] : projects_) {
        for (auto& t : pd.tracks) {
            if (t->id() == id.value) return t.get();
        }
    }
    return nullptr;
}

auto ProjectManager::find_track(TrackID id) const -> const Track* {
    for (const auto& [pid, pd] : projects_) {
        for (const auto& t : pd.tracks) {
            if (t->id() == id.value) return t.get();
        }
    }
    return nullptr;
}

auto ProjectManager::find_project_for_track(TrackID id) -> ProjectData* {
    for (auto& [pid, pd] : projects_) {
        for (auto& t : pd.tracks) {
            if (t->id() == id.value) return &pd;
        }
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Project lifecycle
// ═══════════════════════════════════════════════════════════════

ProjectID ProjectManager::create(const std::string& name,
                                  const std::string& path) {
    ProjectID id{next_project_id_++};
    ProjectData pd;
    pd.name     = name;
    pd.path     = path;
    pd.modified = false;

    projects_.emplace(id, std::move(pd));
    active_project_ = id;
    return id;
}

ProjectID ProjectManager::open(const std::string& path) {
    // Stub: in production this would deserialize from a file.
    // For now we create a new project with the file name.
    auto pos = path.find_last_of("/\\");
    std::string name = (pos != std::string::npos)
                           ? path.substr(pos + 1)
                           : path;
    // Strip extension
    auto dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);

    return create(name, path);
}

bool ProjectManager::save(ProjectID id) {
    auto* pd = find_project(id);
    if (!pd) return false;
    pd->modified = false;
    return true;  // Stub: real serialization TBD
}

bool ProjectManager::close(ProjectID id) {
    auto it = projects_.find(id);
    if (it == projects_.end()) return false;
    projects_.erase(it);

    if (active_project_ == id) {
        active_project_ = projects_.empty() ? INVALID_PROJECT_ID
                                            : projects_.begin()->first;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════
// Track management
// ═══════════════════════════════════════════════════════════════

TrackID ProjectManager::create_track(ProjectID project, TrackType type,
                                      const std::string& name) {
    auto* pd = find_project(project);
    if (!pd) return TrackID{0};

    auto track = std::make_unique<Track>(type);
    track->set_name(name);

    TrackID id{track->id()};
    pd->tracks.push_back(std::move(track));
    pd->modified = true;

    // Push undo snapshot (stub — would store reverse delta)
    pd->undo.push("Create track", []{}, []{});

    return id;
}

bool ProjectManager::delete_track(ProjectID project, TrackID track) {
    auto* pd = find_project(project);
    if (!pd) return false;

    auto it = std::remove_if(pd->tracks.begin(), pd->tracks.end(),
                              [track](const auto& t) {
                                  return t->id() == track.value;
                              });
    if (it == pd->tracks.end()) return false;

    pd->tracks.erase(it, pd->tracks.end());
    pd->modified = true;

    pd->undo.push("Delete track", []{}, []{});
    return true;
}

Track* ProjectManager::get_track(TrackID id) {
    return find_track(id);
}

const Track* ProjectManager::get_track(TrackID id) const {
    return find_track(id);
}

// ═══════════════════════════════════════════════════════════════
// Clip management
// ═══════════════════════════════════════════════════════════════

namespace {
    uint64_t g_next_clip_id = 1;
}

ClipID ProjectManager::create_midi_clip(TrackID track_id,
                                         uint64_t start_ppqn,
                                         uint64_t length_ppqn) {
    auto* pd = find_project_for_track(track_id);
    if (!pd) return ClipID{0};

    ClipID id{g_next_clip_id++};
    // Store clip placement — production would add to track's clip list
    (void)start_ppqn;
    (void)length_ppqn;
    pd->modified = true;
    pd->undo.push("Create MIDI clip", []{}, []{});
    return id;
}

ClipID ProjectManager::create_audio_clip(TrackID track_id,
                                          uint64_t start_ppqn,
                                          const std::string& file) {
    auto* pd = find_project_for_track(track_id);
    if (!pd) return ClipID{0};

    ClipID id{g_next_clip_id++};
    (void)start_ppqn;
    (void)file;
    pd->modified = true;
    pd->undo.push("Create audio clip", []{}, []{});
    return id;
}

bool ProjectManager::delete_clip(ClipID id) {
    // Stub: production would remove from the owning track's clip list
    // and push an undo entry.
    return id.value != 0;
}

// ═══════════════════════════════════════════════════════════════
// Undo
// ═══════════════════════════════════════════════════════════════

UndoStack& ProjectManager::undo(ProjectID id) {
    static UndoStack fallback;
    auto* pd = find_project(id);
    return pd ? pd->undo : fallback;
}

// ═══════════════════════════════════════════════════════════════
// Arrangement
// ═══════════════════════════════════════════════════════════════

Arrangement& ProjectManager::get_arrangement(ProjectID id) {
    static Arrangement fallback;
    auto* pd = find_project(id);
    return pd ? pd->arrangement : fallback;
}

const Arrangement& ProjectManager::get_arrangement(ProjectID id) const {
    static Arrangement fallback;
    const auto* pd = find_project(id);
    return pd ? pd->arrangement : fallback;
}

} // namespace aria
