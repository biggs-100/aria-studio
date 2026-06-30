#include "project_manager.h"

#include "model/session.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
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

    // Session is freed via unique_ptr
    it->second.session.reset();

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

size_t ProjectManager::track_count(ProjectID project) const {
    auto* pd = find_project(project);
    return pd ? pd->tracks.size() : 0;
}

TrackID ProjectManager::create_audio_track(ProjectID project,
                                            const std::string& name) {
    auto* pd = find_project(project);
    if (!pd) return TrackID{0};

    auto track = std::make_unique<AudioTrack>();
    track->set_name(name);

    TrackID id{track->id()};
    pd->tracks.push_back(std::move(track));
    pd->modified = true;
    pd->undo.push("Create audio track", []{}, []{});
    return id;
}

TrackID ProjectManager::create_midi_track(ProjectID project,
                                           const std::string& name) {
    auto* pd = find_project(project);
    if (!pd) return TrackID{0};

    auto track = std::make_unique<MidiTrack>();
    track->set_name(name);

    TrackID id{track->id()};
    pd->tracks.push_back(std::move(track));
    pd->modified = true;
    pd->undo.push("Create MIDI track", []{}, []{});
    return id;
}

TrackID ProjectManager::create_track(ProjectID project, TrackType type,
                                      const std::string& name) {
    auto* pd = find_project(project);
    if (!pd) return TrackID{0};

    // Only one MasterTrack allowed per project
    if (type == TrackType::Master) {
        for (const auto& t : pd->tracks) {
            if (t->type() == TrackType::Master) {
                return TrackID{0};  // Master already exists
            }
        }
    }

    std::unique_ptr<Track> track;
    switch (type) {
        case TrackType::Group:
            track = std::make_unique<GroupTrack>();
            break;
        case TrackType::VCA:
            track = std::make_unique<VCATrack>();
            break;
        case TrackType::Return:
            track = std::make_unique<ReturnTrack>();
            break;
        case TrackType::Audio:
            track = std::make_unique<AudioTrack>();
            break;
        case TrackType::MIDI:
            track = std::make_unique<MidiTrack>();
            break;
        case TrackType::Master:
            track = std::make_unique<MasterTrack>();
            break;
        default:
            track = std::make_unique<Track>(type);
            break;
    }

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

    // Prevent deletion of MasterTrack
    for (const auto& t : pd->tracks) {
        if (t->id() == track.value && t->type() == TrackType::Master) {
            return false;  // Cannot delete master
        }
    }

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
    // Find the track
    auto* track = find_track(track_id);
    if (!track) return ClipID{0};

    // Check track type supports audio clips
    auto type = track->type();
    if (type == TrackType::MIDI || type == TrackType::Return) {
        return ClipID{0};  // MIDI and Return tracks do not support audio clips
    }

    // Create the audio clip
    auto clip = std::make_shared<AudioClip>();
    clip->set_file_path(file);

    // Set clip name from the file name (strip directory)
    auto pos = file.find_last_of("/\\");
    std::string clip_name = (pos != std::string::npos) ? file.substr(pos + 1) : file;
    clip->set_name(clip_name);

    // Place clip on the track
    if (!track->add_clip(clip, start_ppqn)) {
        return ClipID{0};  // Track is frozen
    }

    // Mark project as modified and push undo
    auto* pd = find_project_for_track(track_id);
    if (pd) {
        pd->modified = true;
        pd->undo.push("Create audio clip: " + clip_name, []{}, []{});
    }

    return clip->id();
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

// ═══════════════════════════════════════════════════════════════
// Session access
// ═══════════════════════════════════════════════════════════════

void ProjectManager::create_session() {
    auto* pd = find_project(active_project_);
    if (pd && !pd->session) {
        pd->session = std::make_unique<Session>();
    }
}

Session* ProjectManager::get_session() {
    auto* pd = find_project(active_project_);
    return pd ? pd->session.get() : nullptr;
}

const Session* ProjectManager::get_session() const {
    const auto* pd = find_project(active_project_);
    return pd ? pd->session.get() : nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Mixer bridge
// ═══════════════════════════════════════════════════════════════

void ProjectManager::register_mixer_channel(TrackID track, ChannelID channel) {
    track_to_channel_[track.value] = channel;
}

void ProjectManager::sync_group_routing(Mixer& mixer) {
    // First pass: create buses for GroupTracks in Summing mode
    // and assign child track channels to those buses.
    for (auto& [pid, pd] : projects_) {
        for (auto& track : pd.tracks) {
            if (track->type() != TrackType::Group) continue;
            auto* group = static_cast<GroupTrack*>(track.get());
            if (group->group_mode() != GroupMode::Summing) continue;

            // Create a bus for this group
            BusID bus_id = mixer.create_bus(track->name() + " Bus");

            // Assign the group channel itself to the bus (it will read from it)
            auto ch_it = track_to_channel_.find(track->id().value);
            if (ch_it != track_to_channel_.end()) {
                mixer.assign_channel_to_bus(ch_it->second, bus_id);
            }

            // Assign children to this bus
            for (auto child_id : group->children()) {
                auto child_ch_it = track_to_channel_.find(child_id.value);
                if (child_ch_it != track_to_channel_.end()) {
                    mixer.assign_channel_to_bus(child_ch_it->second, bus_id);
                }
            }
        }
    }

    // Second pass: set up parent bus hierarchy for nested groups
    for (auto& [pid, pd] : projects_) {
        for (auto& track : pd.tracks) {
            if (track->type() != TrackType::Group) continue;
            auto* group = static_cast<GroupTrack*>(track.get());
            if (group->group_mode() != GroupMode::Summing) continue;

            // Find which bus was created for this group
            auto ch_it = track_to_channel_.find(track->id().value);
            if (ch_it == track_to_channel_.end()) continue;
            ChannelID group_ch = ch_it->second;
            BusID group_bus = mixer.channel_bus(group_ch);
            if (group_bus == kInvalidBusID) continue;

            // Check if this track is a child of another group
            for (auto& pt : pd.tracks) {
                if (pt->type() != TrackType::Group) continue;
                if (pt->id() == track->id()) continue;
                auto* parent_group = static_cast<GroupTrack*>(pt.get());
                if (parent_group->group_mode() != GroupMode::Summing) continue;

                const auto& children = parent_group->children();
                if (std::find(children.begin(), children.end(), track->id()) != children.end()) {
                    // This group is a child of parent_group
                    auto parent_ch_it = track_to_channel_.find(pt->id().value);
                    if (parent_ch_it != track_to_channel_.end()) {
                        BusID parent_bus = mixer.channel_bus(parent_ch_it->second);
                        if (parent_bus != kInvalidBusID) {
                            mixer.set_bus_parent(group_bus, parent_bus);
                        }
                    }
                    break;
                }
            }
        }
    }
}

void ProjectManager::sync_vca(Mixer& mixer) {
    for (auto& [pid, pd] : projects_) {
        for (auto& track : pd.tracks) {
            if (track->type() != TrackType::VCA) continue;
            auto* vca = static_cast<VCATrack*>(track.get());

            double vca_volume_db = vca->volume();

            for (auto slave_id : vca->slaves()) {
                // Find the slave track
                auto* slave_track = find_track(slave_id);
                if (!slave_track) continue;

                // Update VCA model-level contribution
                vca->apply_to(slave_id, vca_volume_db);

                // Push to mixer channel
                auto ch_it = track_to_channel_.find(slave_id.value);
                if (ch_it != track_to_channel_.end()) {
                    auto* channel = mixer.get_channel(ch_it->second);
                    if (channel) {
                        // Slave at -∞ stays at -∞ (contribution doesn't unmute)
                        if (slave_track->volume() <= -120.0) {
                            channel->set_vca_contribution(0.0);
                        } else {
                            channel->set_vca_contribution(vca_volume_db);
                        }
                    }
                }
            }
        }
    }
}

} // namespace aria
