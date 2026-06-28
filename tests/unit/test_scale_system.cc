#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/scale_system.h"

using Catch::Approx;
using namespace aria;

// ═══════════════════════════════════════════════════════════════
// Scale definitions
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ScaleSystem built-in scales", "[pianoroll][scale]") {
    SECTION("Major scale has 7 intervals") {
        REQUIRE(ScaleSystem::MAJOR.intervals.size() == 7);
        REQUIRE(ScaleSystem::MAJOR.intervals[0] == 0);
        REQUIRE(ScaleSystem::MAJOR.intervals[3] == 5);  // Perfect fourth
        REQUIRE(ScaleSystem::MAJOR.intervals[6] == 11); // Major seventh
    }

    SECTION("Minor scale has 7 intervals") {
        REQUIRE(ScaleSystem::MINOR.intervals.size() == 7);
        REQUIRE(ScaleSystem::MINOR.intervals[0] == 0);
        REQUIRE(ScaleSystem::MINOR.intervals[2] == 3);   // Minor third
        REQUIRE(ScaleSystem::MINOR.intervals[6] == 10);  // Minor seventh
    }

    SECTION("Pentatonic Major has 5 intervals") {
        REQUIRE(ScaleSystem::PENTATONIC_MAJOR.intervals.size() == 5);
        REQUIRE(ScaleSystem::PENTATONIC_MAJOR.intervals[0] == 0);
        REQUIRE(ScaleSystem::PENTATONIC_MAJOR.intervals[4] == 9);
    }

    SECTION("Blues scale has 6 intervals") {
        REQUIRE(ScaleSystem::BLUES.intervals.size() == 6);
        REQUIRE(ScaleSystem::BLUES.intervals[3] == 6);  // Tritone / blue note
    }

    SECTION("Chromatic scale has 12 intervals") {
        REQUIRE(ScaleSystem::CHROMATIC.intervals.size() == 12);
        for (int i = 0; i < 12; ++i) {
            REQUIRE(ScaleSystem::CHROMATIC.intervals[i] == i);
        }
    }

    SECTION("Dorian is a minor mode with raised 6th") {
        REQUIRE(ScaleSystem::DORIAN.intervals[5] == 9);  // Major sixth
        REQUIRE(ScaleSystem::DORIAN.intervals[3] == 5);  // Perfect fourth
    }

    SECTION("Locrian has diminished fifth") {
        REQUIRE(ScaleSystem::LOCRIAN.intervals[4] == 6);  // Diminished fifth
    }
}

TEST_CASE("ScaleSystem scale names", "[pianoroll][scale]") {
    SECTION("scale_name returns correct name") {
        ScaleSystem ss;
        REQUIRE(ss.scale_name() == "Major");
    }

    SECTION("scale_name after set_scale") {
        ScaleSystem ss;
        ss.set_scale(60, ScaleSystem::MINOR);
        REQUIRE(ss.scale_name() == "Minor");
    }
}

// ═══════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ScaleSystem defaults", "[pianoroll][scale]") {
    ScaleSystem ss;

    SECTION("default root is C4 (60)") {
        REQUIRE(ss.root() == 60);
    }

    SECTION("default scale is Major") {
        REQUIRE(ss.scale_name() == "Major");
    }

    SECTION("default is disabled") {
        REQUIRE_FALSE(ss.is_enabled());
    }
}

TEST_CASE("ScaleSystem set_scale", "[pianoroll][scale]") {
    ScaleSystem ss;

    SECTION("can set root and scale") {
        ss.set_scale(69, ScaleSystem::BLUES);  // A4
        REQUIRE(ss.root() == 69);
        REQUIRE(ss.scale_name() == "Blues");
    }

    SECTION("set_enabled toggles state") {
        ss.set_enabled(true);
        REQUIRE(ss.is_enabled());

        ss.set_enabled(false);
        REQUIRE_FALSE(ss.is_enabled());
    }
}

// ═══════════════════════════════════════════════════════════════
// is_in_scale
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ScaleSystem is_in_scale", "[pianoroll][scale]") {
    ScaleSystem ss;
    ss.set_scale(60, ScaleSystem::MAJOR);  // C Major
    ss.set_enabled(true);

    SECTION("C Major: C, D, E, F, G, A, B are in scale") {
        REQUIRE(ss.is_in_scale(60));  // C4
        REQUIRE(ss.is_in_scale(62));  // D4
        REQUIRE(ss.is_in_scale(64));  // E4
        REQUIRE(ss.is_in_scale(65));  // F4
        REQUIRE(ss.is_in_scale(67));  // G4
        REQUIRE(ss.is_in_scale(69));  // A4
        REQUIRE(ss.is_in_scale(71));  // B4
    }

    SECTION("C Major: C#, D#, F#, G#, A# are NOT in scale") {
        REQUIRE_FALSE(ss.is_in_scale(61));  // C#4
        REQUIRE_FALSE(ss.is_in_scale(63));  // D#4
        REQUIRE_FALSE(ss.is_in_scale(66));  // F#4
        REQUIRE_FALSE(ss.is_in_scale(68));  // G#4
        REQUIRE_FALSE(ss.is_in_scale(70));  // A#4
    }

    SECTION("is_in_scale works across octaves") {
        REQUIRE(ss.is_in_scale(72));  // C5
        REQUIRE_FALSE(ss.is_in_scale(73)); // C#5
        REQUIRE(ss.is_in_scale(74));  // D5
        REQUIRE(ss.is_in_scale(48));  // C3
        REQUIRE_FALSE(ss.is_in_scale(49)); // C#3
    }

    SECTION("C Minor: Eb, Ab, Bb are in scale") {
        ss.set_scale(60, ScaleSystem::MINOR);
        REQUIRE(ss.is_in_scale(63));  // D#4 / Eb4
        REQUIRE(ss.is_in_scale(68));  // G#4 / Ab4
        REQUIRE(ss.is_in_scale(70));  // A#4 / Bb4
        REQUIRE_FALSE(ss.is_in_scale(64)); // E4 not in C minor
    }
}

TEST_CASE("ScaleSystem is_in_scale when disabled", "[pianoroll][scale]") {
    ScaleSystem ss;
    ss.set_scale(60, ScaleSystem::MAJOR);
    ss.set_enabled(false);

    SECTION("all notes return true when disabled") {
        for (uint8_t n = 0; n < 127; ++n) {
            REQUIRE(ss.is_in_scale(n));
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// snap_to_scale
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ScaleSystem snap_to_scale", "[pianoroll][scale]") {
    ScaleSystem ss;
    ss.set_scale(60, ScaleSystem::MAJOR);  // C Major
    ss.set_enabled(true);

    SECTION("scale tones snap to themselves") {
        REQUIRE(ss.snap_to_scale(60) == 60);  // C
        REQUIRE(ss.snap_to_scale(64) == 64);  // E
        REQUIRE(ss.snap_to_scale(67) == 67);  // G
    }

    SECTION("non-scale tones snap to nearest") {
        // C# (61) should snap to C (60) or D (62) — nearest is D? 61-60=1, 62-61=1
        // Both are same distance, so either is valid.
        uint8_t snapped = ss.snap_to_scale(61);
        REQUIRE((snapped == 60 || snapped == 62));
    }

    SECTION("F# (66) in C Major snaps to F (65) or G (67)") {
        uint8_t snapped = ss.snap_to_scale(66);
        REQUIRE((snapped == 65 || snapped == 67));
    }

    SECTION("snap disabled returns original note") {
        ss.set_enabled(false);
        REQUIRE(ss.snap_to_scale(61) == 61);
        REQUIRE(ss.snap_to_scale(66) == 66);
    }
}

// ═══════════════════════════════════════════════════════════════
// scale_degree
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ScaleSystem scale_degree", "[pianoroll][scale]") {
    ScaleSystem ss;
    ss.set_scale(60, ScaleSystem::MAJOR);  // C Major
    ss.set_enabled(true);

    SECTION("root returns 0") {
        REQUIRE(ss.scale_degree(60) == 0);   // C
    }

    SECTION("scale degrees return correct index") {
        REQUIRE(ss.scale_degree(62) == 1);   // D = 2nd degree
        REQUIRE(ss.scale_degree(64) == 2);   // E = 3rd degree
        REQUIRE(ss.scale_degree(65) == 3);   // F = 4th degree
        REQUIRE(ss.scale_degree(67) == 4);   // G = 5th degree
        REQUIRE(ss.scale_degree(69) == 5);   // A = 6th degree
        REQUIRE(ss.scale_degree(71) == 6);   // B = 7th degree
    }

    SECTION("non-scale tone returns -1") {
        REQUIRE(ss.scale_degree(61) == -1);  // C#
        REQUIRE(ss.scale_degree(66) == -1);  // F#
    }

    SECTION("degree works across octaves") {
        // C5 is octave of C4 — both are degree 0
        REQUIRE(ss.scale_degree(72) == 0);
        REQUIRE(ss.scale_degree(84) == 0);  // C6
    }
}

// ═══════════════════════════════════════════════════════════════
// scale_notes
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ScaleSystem scale_notes", "[pianoroll][scale]") {
    ScaleSystem ss;
    ss.set_scale(60, ScaleSystem::MAJOR);  // C Major
    ss.set_enabled(true);

    SECTION("scale_notes in octave 4 only") {
        auto notes = ss.scale_notes(4, 4);
        REQUIRE(notes.size() == 7);
        REQUIRE(notes[0] == 60);  // C4
        REQUIRE(notes[1] == 62);  // D4
        REQUIRE(notes[2] == 64);  // E4
        REQUIRE(notes[3] == 65);  // F4
        REQUIRE(notes[4] == 67);  // G4
        REQUIRE(notes[5] == 69);  // A4
        REQUIRE(notes[6] == 71);  // B4
    }

    SECTION("scale_notes across multiple octaves") {
        auto notes = ss.scale_notes(4, 5);
        // 7 notes in octave 4 + 7 notes in octave 5
        REQUIRE(notes.size() == 14);
    }

    SECTION("C Minor scale notes") {
        ss.set_scale(60, ScaleSystem::MINOR);
        auto notes = ss.scale_notes(4, 4);
        REQUIRE(notes[0] == 60);  // C4
        REQUIRE(notes[2] == 63);  // Eb4 (minor third)
        REQUIRE(notes[4] == 67);  // G4
        REQUIRE(notes[6] == 70);  // Bb4 (minor seventh)
    }

    SECTION("Pentatonic produces 5 notes per octave") {
        ss.set_scale(60, ScaleSystem::PENTATONIC_MAJOR);
        auto notes = ss.scale_notes(4, 4);
        REQUIRE(notes.size() == 5);
    }
}
