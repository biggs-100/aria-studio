#ifndef ARIA_PIANOROLL_GHOST_NOTES_H
#define ARIA_PIANOROLL_GHOST_NOTES_H

#include "note.h"
#include "note_collection.h"
#include "scale_system.h"

#include <cstdint>
#include <vector>

namespace aria {

// Forward declarations.
struct ViewTransform;

/// A single ghost note — rendered at reduced opacity as a visual reference.
struct GhostNote {
    Note    note;        ///< The note data (timing, key, velocity etc.)
    float   opacity;     ///< Per-note opacity override (0.0–1.0)
    Rgba    color;       ///< Per-note colour override

    GhostNote() = default;
    GhostNote(const Note& n, float o = 0.25f, const Rgba& c = Rgba{})
        : note(n), opacity(o), color(c) {}
};

/// Provides ghost (reference) notes from various sources for the piano roll.
///
/// Ghost notes are semi-transparent notes rendered behind the main note grid
/// to provide visual context — adjacent clips, scale tones, chord tones, etc.
class GhostNoteSource {
public:
    /// The type of source for ghost notes.
    enum SourceType : uint8_t {
        SameTrack,      ///< Notes from adjacent clips on the same track
        OtherTrack,     ///< Notes from a user-selected reference track
        Scale,          ///< Scale-degree markers
        Chord,          ///< Current chord tones
        Custom          ///< User-specified reference notes
    };

    // ─── Configuration ────────────────────────────────────────

    /// Set the source type and optional track ID.
    void set_source(SourceType type, uint64_t track = UINT64_MAX);

    /// Set the global opacity for ghost notes (0.0–1.0).
    void set_opacity(float opacity);

    float opacity() const { return opacity_; }
    SourceType source_type() const { return type_; }

    // ─── Generation ───────────────────────────────────────────

    /// Generate ghost notes based on the current source type.
    std::vector<Note> get_ghost_notes(const NoteCollection& current,
                                       const ViewTransform& view,
                                       const ScaleSystem& scale) const;

private:
    SourceType type_     = Scale;
    float      opacity_  = 0.25f;
    uint64_t   track_id_ = UINT64_MAX;

    /// Generate ghost notes from scale tones.
    std::vector<Note> generate_scale_ghosts(const ViewTransform& view,
                                             const ScaleSystem& scale) const;

    /// Generate ghost notes from the same track (adjacent clip context).
    std::vector<Note> generate_same_track_ghosts(
        const NoteCollection& current, const ViewTransform& view) const;

    /// Generate ghost notes from chord tones.
    std::vector<Note> generate_chord_ghosts(const NoteCollection& current,
                                             const ScaleSystem& scale) const;
};

} // namespace aria

#endif // ARIA_PIANOROLL_GHOST_NOTES_H
