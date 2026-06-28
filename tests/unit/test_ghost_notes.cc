#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/ghost_notes.h"
#include "pianoroll/scale_system.h"
#include "pianoroll/piano_roll_canvas.h"  // For ViewTransform

using Catch::Approx;
using namespace aria;

// ═══════════════════════════════════════════════════════════════
// Source type and configuration
// ═══════════════════════════════════════════════════════════════

TEST_CASE("GhostNoteSource defaults", "[pianoroll][ghost]") {
    GhostNoteSource gns;

    SECTION("default source type is Scale") {
        REQUIRE(gns.source_type() == GhostNoteSource::Scale);
    }

    SECTION("default opacity is 0.25") {
        REQUIRE(gns.opacity() == Approx(0.25f));
    }
}

TEST_CASE("GhostNoteSource configuration", "[pianoroll][ghost]") {
    GhostNoteSource gns;

    SECTION("set_source changes type") {
        gns.set_source(GhostNoteSource::SameTrack, 42);
        REQUIRE(gns.source_type() == GhostNoteSource::SameTrack);
    }

    SECTION("set_opacity clamps to 0-1 range") {
        gns.set_opacity(2.0f);
        REQUIRE(gns.opacity() == Approx(1.0f));

        gns.set_opacity(-1.0f);
        REQUIRE(gns.opacity() == Approx(0.0f));
    }

    SECTION("set_opacity within range") {
        gns.set_opacity(0.5f);
        REQUIRE(gns.opacity() == Approx(0.5f));
    }
}

// ═══════════════════════════════════════════════════════════════
// Scale ghost generation
// ═══════════════════════════════════════════════════════════════

TEST_CASE("GhostNoteSource scale ghosts", "[pianoroll][ghost][scale]") {
    GhostNoteSource gns;
    gns.set_source(GhostNoteSource::Scale);

    ScaleSystem scale;
    scale.set_scale(60, ScaleSystem::MAJOR);  // C Major
    scale.set_enabled(true);

    ViewTransform view;
    view.ppqn_per_pixel_x = 4.0;
    view.pixels_per_semitone_y = 8.0;
    view.scroll_ppqn = 0.0;
    view.scroll_key = 48.0;  // Starting around C3

    NoteCollection empty_collection;

    SECTION("ghosts generated for scale tones in view") {
        auto ghosts = gns.get_ghost_notes(empty_collection, view, scale);

        // Should return scale tones for visible keys.
        // With scroll_key=48 (C3) and 1000px/8px-per-semitone=125 semitones visible,
        // we should see many scale tones.
        REQUIRE_FALSE(ghosts.empty());
    }

    SECTION("ghosts have low opacity") {
        gns.set_opacity(0.25f);
        auto ghosts = gns.get_ghost_notes(empty_collection, view, scale);

        if (!ghosts.empty()) {
            REQUIRE(ghosts[0].opacity == Approx(0.25f));
        }
    }

    SECTION("ghosts have zero velocity (non-playable)") {
        auto ghosts = gns.get_ghost_notes(empty_collection, view, scale);

        for (const auto& g : ghosts) {
            REQUIRE(g.velocity == 0);
        }
    }

    SECTION("no ghosts when scale disabled") {
        scale.set_enabled(false);
        auto ghosts = gns.get_ghost_notes(empty_collection, view, scale);
        REQUIRE(ghosts.empty());
    }
}

// ═══════════════════════════════════════════════════════════════
// Chord ghost generation
// ═══════════════════════════════════════════════════════════════

TEST_CASE("GhostNoteSource chord ghosts", "[pianoroll][ghost][chord]") {
    GhostNoteSource gns;
    gns.set_source(GhostNoteSource::Chord);

    ScaleSystem scale;
    scale.set_scale(60, ScaleSystem::MAJOR);  // C Major
    scale.set_enabled(true);

    ViewTransform view;
    view.ppqn_per_pixel_x = 4.0;
    view.pixels_per_semitone_y = 8.0;
    view.scroll_ppqn = 0.0;
    view.scroll_key = 48.0;

    NoteCollection empty_collection;

    SECTION("chord ghosts generated for chord tones (1,3,5,7 degrees)") {
        auto ghosts = gns.get_ghost_notes(empty_collection, view, scale);

        // In C Major, chord tones are C, E, G, B
        // We should have at least these 4 chord tone markers.
        REQUIRE(ghosts.size() >= 4);
    }

    SECTION("chord ghosts have chord-specific colour") {
        auto ghosts = gns.get_ghost_notes(empty_collection, view, scale);

        if (!ghosts.empty()) {
            // Chord ghosts should have a different tint than scale ghosts.
            REQUIRE(ghosts[0].color.r == Approx(0.7f));
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Same-track and other sources
// ═══════════════════════════════════════════════════════════════

TEST_CASE("GhostNoteSource other sources", "[pianoroll][ghost][sources]") {
    GhostNoteSource gns;
    ScaleSystem scale;
    ViewTransform view;
    NoteCollection empty;

    SECTION("SameTrack returns empty (not yet implemented)") {
        gns.set_source(GhostNoteSource::SameTrack);
        auto ghosts = gns.get_ghost_notes(empty, view, scale);
        REQUIRE(ghosts.empty());
    }

    SECTION("OtherTrack returns empty (not yet implemented)") {
        gns.set_source(GhostNoteSource::OtherTrack, 1);
        auto ghosts = gns.get_ghost_notes(empty, view, scale);
        REQUIRE(ghosts.empty());
    }

    SECTION("Custom returns empty (not yet implemented)") {
        gns.set_source(GhostNoteSource::Custom);
        auto ghosts = gns.get_ghost_notes(empty, view, scale);
        REQUIRE(ghosts.empty());
    }
}
