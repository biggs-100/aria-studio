#ifndef ARIA_TRACK_TYPES_H
#define ARIA_TRACK_TYPES_H

#include "model/track.h"
#include "model/audio_clip.h"
#include "midi/midi_clip.h"

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aria {

// ─── Crossfade ──────────────────────────────────────────────────

/// Describes a crossfade transition between two overlapping clips.
struct Crossfade {
    ClipID    clip_a;
    ClipID    clip_b;
    uint32_t  duration_ppqn = 0;
    FadeShape shape         = FadeShape::EqualPowerOut;
};

/// Gain values for the two clips at a crossfade evaluation point.
struct CrossfadeGain {
    double gain_a = 0.0;
    double gain_b = 0.0;
};

// ─── AudioTrack ────────────────────────────────────────────────

/// A track that owns typed AudioClip storage, crossfade management,
/// and session view clip slots.
class AudioTrack : public Track {
public:
    explicit AudioTrack();

    // ─── Typed clip management ───────────────────────────────
    /// Add an AudioClip at the given PPQN position.
    /// Returns false if the track is frozen.
    bool add_audio_clip(std::shared_ptr<AudioClip> clip, uint64_t start_ppqn);

    /// Get the number of audio clips.
    size_t audio_clip_count() const;

    /// Find the AudioClip at a given PPQN position (nullptr if none).
    AudioClip* audio_clip_at(uint64_t ppqn) const;

    /// Find all AudioClips overlapping the given PPQN range.
    std::vector<AudioClip*> clips_in_range(uint64_t start_ppqn,
                                           uint64_t end_ppqn) const;

    // ─── Crossfade management ─────────────────────────────────
    /// Define a crossfade between two overlapping clips.
    void add_crossfade(ClipID a, ClipID b, uint32_t duration_ppqn,
                       FadeShape shape);

    /// Remove a crossfade between two clips.
    void remove_crossfade(ClipID a, ClipID b);

    /// Get the number of crossfades.
    size_t crossfade_count() const;

    /// Evaluate crossfade gains at a given PPQN position.
    /// Returns nullopt if no crossfade covers that position.
    std::optional<CrossfadeGain> evaluate_crossfade(uint64_t ppqn) const;

    // ─── Session slots ────────────────────────────────────────
    /// Place a clip in a session scene slot.
    void set_clip_in_slot(SceneID scene, std::shared_ptr<Clip> clip);

    /// Get the clip in a session scene slot (nullptr if empty).
    Clip* clip_in_slot(SceneID scene) const;

private:
    std::vector<std::shared_ptr<AudioClip>> audio_clips_;
    std::unordered_map<std::pair<ClipID, ClipID>, Crossfade> crossfade_map_;
    std::unordered_map<SceneID, std::shared_ptr<Clip>> session_slots_;
};

// ─── MidiTrack ──────────────────────────────────────────────────

/// A track that owns MIDI clips with instrument assignment,
/// transpose offset, drum mapping, and session clip slots.
class MidiTrack : public Track {
public:
    explicit MidiTrack();

    // ─── Instrument ───────────────────────────────────────────
    /// Set the instrument plugin ID.
    void set_instrument(const std::string& instrument);

    /// Get the instrument plugin ID (empty if none).
    const std::string& instrument() const;

    // ─── Transpose ────────────────────────────────────────────
    /// Set the transpose offset in semitones.
    void set_transpose(int8_t semitones);

    /// Get the transpose offset.
    int8_t transpose() const;

    // ─── Drum map ─────────────────────────────────────────────
    /// Set the drum note mapping (source → target).
    void set_drum_mapping(const std::unordered_map<uint8_t, uint8_t>& mapping);

    /// Map a MIDI note through the drum map.
    /// Returns the mapped note if mapped, or the original if not.
    uint8_t map_drum_note(uint8_t note) const;

    // ─── Session slots ────────────────────────────────────────
    /// Place a clip in a session scene slot.
    void set_clip_in_slot(SceneID scene, std::shared_ptr<Clip> clip);

    /// Get the clip in a session scene slot (nullptr if empty).
    Clip* clip_in_slot(SceneID scene) const;

private:
    std::string instrument_;
    int8_t transpose_ = 0;
    std::unordered_map<uint8_t, uint8_t> drum_map_;
    std::unordered_map<SceneID, std::shared_ptr<Clip>> session_slots_;
};

// ─── GroupMode ────────────────────────────────────────────────

/// How a GroupTrack aggregates its children.
enum class GroupMode : uint8_t {
    Summing,  ///< Traditional group bus — sums child audio
    Folder,   ///< Visual container only — no audio summing
    Routing   ///< Children route individually
};

// ─── VCAAffects ───────────────────────────────────────────────

/// Which parameters a VCATrack controls on its slaves.
enum class VCAAffects : uint8_t {
    Volume,     ///< Volume only
    VolumePan,  ///< Volume + pan
    All         ///< Volume + pan + sends
};

// ─── GroupTrack ──────────────────────────────────────────────

/// A track that groups child tracks with configurable summing mode.
///
/// Modes:
/// - Summing: children sum into this track's bus (traditional group)
/// - Folder: visual container only, no audio processing
/// - Routing: children route individually (group is organizational)
class GroupTrack : public Track {
public:
    explicit GroupTrack();

    /// Add a child track to this group.
    void add_child(TrackID child);

    /// Remove a child track from this group.
    void remove_child(TrackID child);

    /// Get the list of child track IDs.
    const std::vector<TrackID>& children() const;

    /// Set the group mode (Summing, Folder, Routing).
    void set_group_mode(GroupMode mode);

    /// Get the group mode.
    GroupMode group_mode() const;

    /// Set the folded state.
    void set_folded(bool folded);

    /// Check if the group is folded.
    bool is_folded() const;

private:
    std::vector<TrackID> children_;
    GroupMode group_mode_ = GroupMode::Summing;
    bool folded_ = false;
};

// ─── VCATrack ────────────────────────────────────────────────

/// A track that controls slave faders proportionally.
///
/// When the VCA fader moves, all slaves' effective volume changes
/// by the same amount. Slaves at -∞ stay at -∞.
class VCATrack : public Track {
public:
    explicit VCATrack();

    /// Add a slave track to this VCA.
    void add_slave(TrackID slave);

    /// Remove a slave track.
    void remove_slave(TrackID slave);

    /// Get the list of slave track IDs.
    const std::vector<TrackID>& slaves() const;

    /// Set which parameters this VCA affects.
    void set_affects(VCAAffects affects);

    /// Get which parameters this VCA affects.
    VCAAffects affects() const;

    /// Link a slave (register for VCA contribution tracking).
    void link_slave(TrackID slave);

    /// Get the VCA contribution for a given slave.
    double vca_contribution(TrackID slave) const;

    /// Apply a VCA volume change to a slave.
    /// Returns the delta applied.
    double apply_to(TrackID slave, double vca_volume_db);

private:
    std::vector<TrackID> slaves_;
    VCAAffects affects_ = VCAAffects::All;
    std::unordered_map<TrackID, double> vca_contributions_;
};

// ─── ReturnTrack ─────────────────────────────────────────────

/// An FX return track with solo-safe behavior.
///
/// Always routes to Master, has no clips, and is never affected
/// by track solo operations.
class ReturnTrack : public Track {
public:
    explicit ReturnTrack();

    /// Set solo-safe behavior.
    void set_solo_safe(bool safe);

    /// Check if solo-safe is enabled.
    bool is_solo_safe() const;

    /// ReturnTrack has no clips.
    bool has_clips() const;

private:
    bool solo_safe_ = true;
};

// ─── MasterTrack ─────────────────────────────────────────────

/// The master output track.
///
/// Exactly one per project. Cannot be deleted.
class MasterTrack : public Track {
public:
    explicit MasterTrack();
};

} // namespace aria

#endif // ARIA_TRACK_TYPES_H
