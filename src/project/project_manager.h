#ifndef ARIA_PROJECT_MANAGER_H
#define ARIA_PROJECT_MANAGER_H

#include "project/arrangement.h"
#include "kernel/undo_stack.h"
#include "model/track.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aria {

// ─── ID types ─────────────────────────────────────────────────

/// Project identifier.
struct ProjectID {
    uint64_t value = 0;
    bool operator==(const ProjectID& o) const { return value == o.value; }
    bool operator!=(const ProjectID& o) const { return value != o.value; }
};

static constexpr ProjectID INVALID_PROJECT_ID{0};

/// Clip identifier.
struct ClipID {
    uint64_t value = 0;
    bool operator==(const ClipID& o) const { return value == o.value; }
    bool operator!=(const ClipID& o) const { return value != o.value; }
};

} // namespace aria

// ─── Hash specializations (BEFORE unordered_map use) ──────────
namespace std {
template<> struct hash<aria::ProjectID> {
    size_t operator()(const aria::ProjectID& id) const noexcept {
        return hash<uint64_t>{}(id.value);
    }
};
} // namespace std

namespace aria {

/// Manages all open projects, their tracks, clips, and undo stacks.
///
/// Provides the high-level API for creating/opening/saving projects,
/// managing tracks and clips, and integrating with the undo system.
class ProjectManager {
public:
    ProjectManager() = default;
    ~ProjectManager() = default;

    // ─── Project lifecycle ────────────────────────────────────

    /// Create a new project with the given name at the given path.
    ProjectID create(const std::string& name, const std::string& path);

    /// Open an existing project from a file.
    ProjectID open(const std::string& path);

    /// Save the project with the given ID.
    bool save(ProjectID id);

    /// Close a project, freeing its resources.
    bool close(ProjectID id);

    /// Get the number of open projects.
    size_t project_count() const { return projects_.size(); }

    /// Set the active (focused) project.
    void set_active_project(ProjectID id) { active_project_ = id; }

    /// Get the active project ID.
    ProjectID active_project() const { return active_project_; }

    // ─── Track management ─────────────────────────────────────

    /// Create a new track in the given project.
    TrackID create_track(ProjectID project, TrackType type,
                         const std::string& name);

    /// Delete a track from the given project.
    bool delete_track(ProjectID project, TrackID track);

    /// Get a track by ID (nullptr if not found).
    Track* get_track(TrackID id);

    /// Const overload.
    const Track* get_track(TrackID id) const;

    // ─── Clip management ──────────────────────────────────────

    ClipID create_midi_clip(TrackID track, uint64_t start_ppqn,
                            uint64_t length_ppqn);
    ClipID create_audio_clip(TrackID track, uint64_t start_ppqn,
                             const std::string& file);
    bool delete_clip(ClipID id);

    // ─── Undo integration ─────────────────────────────────────

    /// Access the undo stack for the given project.
    UndoStack& undo(ProjectID id);

    // ─── Arrangement access ───────────────────────────────────

    /// Get the arrangement for a project.
    Arrangement& get_arrangement(ProjectID id);
    const Arrangement& get_arrangement(ProjectID id) const;

private:
    /// Per-project data.
    struct ProjectData {
        std::string name;
        std::string path;
        std::vector<std::unique_ptr<Track>> tracks;
        Arrangement arrangement;
        UndoStack   undo;
        bool        modified = false;
    };

    std::unordered_map<ProjectID, ProjectData> projects_;
    ProjectID active_project_{INVALID_PROJECT_ID};
    uint64_t next_project_id_ = 1;

    /// Internal: find project data (nullptr if not found).
    ProjectData* find_project(ProjectID id);
    const ProjectData* find_project(ProjectID id) const;

    /// Internal: find track across all projects (nullptr if not found).
    Track* find_track(TrackID id);
    const Track* find_track(TrackID id) const;

    /// Internal: find which project owns a track.
    ProjectData* find_project_for_track(TrackID id);
};

} // namespace aria

#endif // ARIA_PROJECT_MANAGER_H
