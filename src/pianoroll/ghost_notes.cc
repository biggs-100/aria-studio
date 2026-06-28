#include "ghost_notes.h"
#include "piano_roll_canvas.h"  // For ViewTransform

#include <algorithm>
#include <cmath>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════

void GhostNoteSource::set_source(SourceType type, uint64_t track) {
    type_ = type;
    track_id_ = track;
}

void GhostNoteSource::set_opacity(float opacity) {
    opacity_ = std::clamp(opacity, 0.0f, 1.0f);
}

// ═══════════════════════════════════════════════════════════════
// Ghost note generation
// ═══════════════════════════════════════════════════════════════

std::vector<Note> GhostNoteSource::get_ghost_notes(
    const NoteCollection& current,
    const ViewTransform& view,
    const ScaleSystem& scale) const
{
    switch (type_) {
    case SameTrack:
        return generate_same_track_ghosts(current, view);
    case OtherTrack:
        // OtherTrack ghosts require external data — currently returns empty.
        return {};
    case Scale:
        return generate_scale_ghosts(view, scale);
    case Chord:
        return generate_chord_ghosts(current, scale);
    case Custom:
        // Custom ghosts require user-provided data — currently returns empty.
        return {};
    default:
        return {};
    }
}

// ═══════════════════════════════════════════════════════════════
// Scale ghosts
// ═══════════════════════════════════════════════════════════════

std::vector<Note> GhostNoteSource::generate_scale_ghosts(
    const ViewTransform& view,
    const ScaleSystem& scale) const
{
    std::vector<Note> ghosts;

    if (!scale.is_enabled()) return ghosts;

    // Determine the visible key range from the view transform.
    double visible_top    = view.scroll_key;
    double visible_bottom = view.scroll_key + (1000.0 / view.pixels_per_semitone_y);
    uint8_t key_low  = static_cast<uint8_t>(std::clamp(
        static_cast<int>(std::floor(visible_top)), 0, 127));
    uint8_t key_high = static_cast<uint8_t>(std::clamp(
        static_cast<int>(std::ceil(visible_bottom)), 0, 127));

    // Determine the visible PPQN range.
    uint64_t ppqn_low  = static_cast<uint64_t>(std::max(0.0, view.scroll_ppqn));
    uint64_t ppqn_high = ppqn_low + static_cast<uint64_t>(1920.0 * view.ppqn_per_pixel_x);

    // For each visible octave, add ghost notes for scale tones.
    uint8_t oct_low  = key_low / 12;
    uint8_t oct_high = key_high / 12;

    std::vector<uint8_t> scale_tones = scale.scale_notes(oct_low, oct_high);
    for (uint8_t note_num : scale_tones) {
        if (note_num >= key_low && note_num <= key_high) {
            Note ghost_note;
            ghost_note.key            = note_num;
            ghost_note.start_ppqn     = ppqn_low;
            ghost_note.duration_ppqn  = ppqn_high - ppqn_low;
            ghost_note.velocity       = 0;       // Ghost — not playable
            ghost_note.opacity        = opacity_;

            // Desaturated colour for ghost notes.
            ghost_note.color = Rgba{0.6f, 0.6f, 0.7f, opacity_};

            ghosts.push_back(ghost_note);
        }
    }

    return ghosts;
}

// ═══════════════════════════════════════════════════════════════
// Same-track ghosts
// ═══════════════════════════════════════════════════════════════

std::vector<Note> GhostNoteSource::generate_same_track_ghosts(
    const NoteCollection& current,
    const ViewTransform& /*view*/) const
{
    // Same-track ghosts show notes from adjacent clips.
    // This requires access to the clip list — for now, return empty.
    // TODO(P4): Implement when clip navigation is available.
    (void)current;
    return {};
}

// ═══════════════════════════════════════════════════════════════
// Chord ghosts
// ═══════════════════════════════════════════════════════════════

std::vector<Note> GhostNoteSource::generate_chord_ghosts(
    const NoteCollection& current,
    const ScaleSystem& scale) const
{
    std::vector<Note> ghosts;

    if (!scale.is_enabled()) return ghosts;

    // Determine chord tones from the scale: 1st, 3rd, 5th, 7th degrees.
    uint8_t root_pc = scale.root() % 12;
    int root_oct = scale.root() / 12;

    // Generate chord ghost notes across the visible range.
    // Highlight scale degrees 0 (root), 2 (third), 4 (fifth), 6 (seventh).
    int chord_degrees[] = {0, 2, 4, 6};

    for (int deg : chord_degrees) {
        auto intervals = scale.scale().intervals;
        if (deg >= static_cast<int>(intervals.size())) continue;

        int semitone = intervals[deg];
        int note_num = root_oct * 12 + static_cast<int>(root_pc) + semitone;

        if (note_num >= 0 && note_num <= 127) {
            Note ghost_note;
            ghost_note.key            = static_cast<uint8_t>(note_num);
            ghost_note.start_ppqn     = 0;
            ghost_note.duration_ppqn  = 1;  // Markers only
            ghost_note.velocity       = 0;
            ghost_note.opacity        = opacity_;

            // Slightly different tint for chord tones vs scale ghosts.
            ghost_note.color = Rgba{0.7f, 0.5f, 0.8f, opacity_};

            ghosts.push_back(ghost_note);
        }
    }

    return ghosts;
}

} // namespace aria
