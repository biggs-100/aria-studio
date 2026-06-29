#include "track_types.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// AudioTrack
// ═══════════════════════════════════════════════════════════════

AudioTrack::AudioTrack()
    : Track(TrackType::Audio) {}

bool AudioTrack::add_audio_clip(std::shared_ptr<AudioClip> clip,
                                 uint64_t start_ppqn) {
    if (is_frozen()) return false;
    audio_clips_.push_back(std::move(clip));
    // Also register in the base class clip list for general queries
    // We add a ClipPlacement wrapping the AudioClip for base clip_at
    Track::add_clip(audio_clips_.back(), start_ppqn);
    return true;
}

size_t AudioTrack::audio_clip_count() const {
    return audio_clips_.size();
}

AudioClip* AudioTrack::audio_clip_at(uint64_t ppqn) const {
    for (const auto& clip : audio_clips_) {
        uint64_t start = 0;  // AudioClip has no placement start; use placement
        // Find the matching ClipPlacement in base class
        for (const auto& cp : clips()) {
            if (cp.clip.get() == clip.get()) {
                start = cp.start_ppqn;
                break;
            }
        }
        uint64_t end = start + clip->length();
        if (ppqn >= start && ppqn < end) {
            return clip.get();
        }
    }
    return nullptr;
}

std::vector<AudioClip*> AudioTrack::clips_in_range(uint64_t start_ppqn,
                                                     uint64_t end_ppqn) const {
    std::vector<AudioClip*> result;
    for (const auto& clip : audio_clips_) {
        uint64_t start = 0;
        for (const auto& cp : clips()) {
            if (cp.clip.get() == clip.get()) {
                start = cp.start_ppqn;
                break;
            }
        }
        uint64_t end = start + clip->length();
        // Overlap test: ranges [start, end) and [start_ppqn, end_ppqn)
        if (end > start_ppqn && start < end_ppqn) {
            result.push_back(clip.get());
        }
    }
    return result;
}

void AudioTrack::add_crossfade(ClipID a, ClipID b, uint32_t duration_ppqn,
                                FadeShape shape) {
    auto key = std::make_pair(a, b);
    crossfade_map_[key] = Crossfade{a, b, duration_ppqn, shape};
}

void AudioTrack::remove_crossfade(ClipID a, ClipID b) {
    crossfade_map_.erase(std::make_pair(a, b));
}

size_t AudioTrack::crossfade_count() const {
    return crossfade_map_.size();
}

std::optional<CrossfadeGain> AudioTrack::evaluate_crossfade(
    uint64_t ppqn) const {
    for (const auto& [pair, xfade] : crossfade_map_) {
        // Find the start PPQN of each clip
        uint64_t start_a = 0, start_b = 0;
        for (const auto& cp : clips()) {
            if (cp.clip->id() == xfade.clip_a) start_a = cp.start_ppqn;
            if (cp.clip->id() == xfade.clip_b) start_b = cp.start_ppqn;
        }

        // Find the clip lengths
        uint64_t len_a = 0, len_b = 0;
        for (const auto& clip : audio_clips_) {
            if (clip->id() == xfade.clip_a) len_a = clip->length();
            if (clip->id() == xfade.clip_b) len_b = clip->length();
        }

        uint64_t end_a = start_a + len_a;
        uint64_t end_b = start_b + len_b;

        // The overlap is the shared region: max(start_a, start_b) to min(end_a, end_b)
        uint64_t overlap_start = std::max(start_a, start_b);
        uint64_t overlap_end   = std::min(end_a, end_b);

        if (ppqn < overlap_start || ppqn > overlap_end) continue;

        // t ∈ [0, 1] within the overlap
        double t = static_cast<double>(ppqn - overlap_start)
                 / static_cast<double>(overlap_end - overlap_start);

        CrossfadeGain result;
        // Clip A fades out: EqualPowerOut or LinearOut shape depending on xfade
        result.gain_a = Clip::evaluate_fade(xfade.shape, t);
        // Clip B fades in: use the complementary shape (EqualPowerIn or LinearIn)
        FadeShape in_shape = FadeShape::EqualPowerIn;
        if (xfade.shape == FadeShape::LinearOut) in_shape = FadeShape::LinearIn;
        result.gain_b = Clip::evaluate_fade(in_shape, t);

        return result;
    }
    return std::nullopt;
}

void AudioTrack::set_clip_in_slot(SceneID scene, std::shared_ptr<Clip> clip) {
    session_slots_[scene] = std::move(clip);
}

Clip* AudioTrack::clip_in_slot(SceneID scene) const {
    auto it = session_slots_.find(scene);
    if (it != session_slots_.end()) return it->second.get();
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// MidiTrack
// ═══════════════════════════════════════════════════════════════

MidiTrack::MidiTrack()
    : Track(TrackType::MIDI) {}

void MidiTrack::set_instrument(const std::string& instrument) {
    instrument_ = instrument;
}

const std::string& MidiTrack::instrument() const {
    return instrument_;
}

void MidiTrack::set_transpose(int8_t semitones) {
    transpose_ = semitones;
}

int8_t MidiTrack::transpose() const {
    return transpose_;
}

void MidiTrack::set_drum_mapping(
    const std::unordered_map<uint8_t, uint8_t>& mapping) {
    drum_map_ = mapping;
}

uint8_t MidiTrack::map_drum_note(uint8_t note) const {
    auto it = drum_map_.find(note);
    if (it != drum_map_.end()) return it->second;
    return note;
}

void MidiTrack::set_clip_in_slot(SceneID scene, std::shared_ptr<Clip> clip) {
    session_slots_[scene] = std::move(clip);
}

Clip* MidiTrack::clip_in_slot(SceneID scene) const {
    auto it = session_slots_.find(scene);
    if (it != session_slots_.end()) return it->second.get();
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// GroupTrack
// ═══════════════════════════════════════════════════════════════

GroupTrack::GroupTrack()
    : Track(TrackType::Group) {}

void GroupTrack::add_child(TrackID child) {
    children_.push_back(child);
}

void GroupTrack::remove_child(TrackID child) {
    auto it = std::remove(children_.begin(), children_.end(), child);
    children_.erase(it, children_.end());
}

const std::vector<TrackID>& GroupTrack::children() const {
    return children_;
}

void GroupTrack::set_group_mode(GroupMode mode) {
    group_mode_ = mode;
}

GroupMode GroupTrack::group_mode() const {
    return group_mode_;
}

void GroupTrack::set_folded(bool folded) {
    folded_ = folded;
}

bool GroupTrack::is_folded() const {
    return folded_;
}

// ═══════════════════════════════════════════════════════════════
// VCATrack
// ═══════════════════════════════════════════════════════════════

VCATrack::VCATrack()
    : Track(TrackType::VCA) {}

void VCATrack::add_slave(TrackID slave) {
    slaves_.push_back(slave);
}

void VCATrack::remove_slave(TrackID slave) {
    auto it = std::remove(slaves_.begin(), slaves_.end(), slave);
    slaves_.erase(it, slaves_.end());
}

const std::vector<TrackID>& VCATrack::slaves() const {
    return slaves_;
}

void VCATrack::set_affects(VCAAffects affects) {
    affects_ = affects;
}

VCAAffects VCATrack::affects() const {
    return affects_;
}

void VCATrack::link_slave(TrackID slave) {
    if (vca_contributions_.find(slave) == vca_contributions_.end()) {
        vca_contributions_[slave] = 0.0;
    }
}

double VCATrack::vca_contribution(TrackID slave) const {
    auto it = vca_contributions_.find(slave);
    if (it != vca_contributions_.end()) return it->second;
    return 0.0;
}

double VCATrack::apply_to(TrackID slave, double vca_volume_db) {
    double contribution = vca_volume_db;
    vca_contributions_[slave] = contribution;
    return contribution;
}

// ═══════════════════════════════════════════════════════════════
// ReturnTrack
// ═══════════════════════════════════════════════════════════════

ReturnTrack::ReturnTrack()
    : Track(TrackType::Return) {}

void ReturnTrack::set_solo_safe(bool safe) {
    solo_safe_ = safe;
}

bool ReturnTrack::is_solo_safe() const {
    return solo_safe_;
}

bool ReturnTrack::has_clips() const {
    return false;
}

// ═══════════════════════════════════════════════════════════════
// MasterTrack
// ═══════════════════════════════════════════════════════════════

MasterTrack::MasterTrack()
    : Track(TrackType::Master) {}

} // namespace aria
