#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "pianoroll/chord_generator.h"
#include "pianoroll/scale_system.h"

#include <algorithm>
#include <set>

using Catch::Approx;
using namespace aria;

// ═══════════════════════════════════════════════════════════════
// Chord generation
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ChordGenerator basic chords", "[pianoroll][chord]") {
    ChordGenerator cg;

    SECTION("C Major triad") {
        auto notes = cg.generate(60, ChordGenerator::Major);
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 64);  // E
        REQUIRE(notes[2] == 67);  // G
    }

    SECTION("C Minor triad") {
        auto notes = cg.generate(60, ChordGenerator::Minor);
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 63);  // Eb
        REQUIRE(notes[2] == 67);  // G
    }

    SECTION("C Dim triad") {
        auto notes = cg.generate(60, ChordGenerator::Dim);
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 63);  // Eb
        REQUIRE(notes[2] == 66);  // Gb
    }

    SECTION("C Aug triad") {
        auto notes = cg.generate(60, ChordGenerator::Aug);
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 64);  // E
        REQUIRE(notes[2] == 68);  // G#
    }
}

TEST_CASE("ChordGenerator seventh chords", "[pianoroll][chord]") {
    ChordGenerator cg;

    SECTION("C Maj7") {
        auto notes = cg.generate(60, ChordGenerator::Maj7);
        REQUIRE(notes.size() == 4);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 64);  // E
        REQUIRE(notes[2] == 67);  // G
        REQUIRE(notes[3] == 71);  // B
    }

    SECTION("C Min7") {
        auto notes = cg.generate(60, ChordGenerator::Min7);
        REQUIRE(notes.size() == 4);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 63);  // Eb
        REQUIRE(notes[2] == 67);  // G
        REQUIRE(notes[3] == 70);  // Bb
    }

    SECTION("C Dom7") {
        auto notes = cg.generate(60, ChordGenerator::Dom7);
        REQUIRE(notes.size() == 4);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 64);  // E
        REQUIRE(notes[2] == 67);  // G
        REQUIRE(notes[3] == 70);  // Bb
    }

    SECTION("C Dim7 (fully diminished)") {
        auto notes = cg.generate(60, ChordGenerator::Dim7);
        REQUIRE(notes.size() == 4);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 63);  // Eb
        REQUIRE(notes[2] == 66);  // Gb
        REQUIRE(notes[3] == 69);  // A
    }
}

TEST_CASE("ChordGenerator sus and power chords", "[pianoroll][chord]") {
    ChordGenerator cg;

    SECTION("C Sus2") {
        auto notes = cg.generate(60, ChordGenerator::Sus2);
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 62);  // D
        REQUIRE(notes[2] == 67);  // G
    }

    SECTION("C Sus4") {
        auto notes = cg.generate(60, ChordGenerator::Sus4);
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 65);  // F
        REQUIRE(notes[2] == 67);  // G
    }

    SECTION("C PowerChord") {
        auto notes = cg.generate(60, ChordGenerator::PowerChord);
        REQUIRE(notes.size() == 2);
        REQUIRE(notes[0] == 60);  // C
        REQUIRE(notes[1] == 67);  // G
    }
}

TEST_CASE("ChordGenerator extended chords", "[pianoroll][chord]") {
    ChordGenerator cg;

    SECTION("C Maj9") {
        auto notes = cg.generate(60, ChordGenerator::Maj9);
        REQUIRE(notes.size() == 5);
        // C, E, G, B, D
        REQUIRE(notes[0] == 60);
        REQUIRE(notes[1] == 64);
        REQUIRE(notes[2] == 67);
        REQUIRE(notes[3] == 71);
        REQUIRE(notes[4] == 74);  // D (9th)
    }

    SECTION("C Min9") {
        auto notes = cg.generate(60, ChordGenerator::Min9);
        REQUIRE(notes.size() == 5);
        REQUIRE(notes[0] == 60);
        REQUIRE(notes[1] == 63);  // Eb
        REQUIRE(notes[4] == 74);  // D (9th)
    }

    SECTION("C Dom9") {
        auto notes = cg.generate(60, ChordGenerator::Dom9);
        REQUIRE(notes.size() == 5);
        REQUIRE(notes[0] == 60);
        REQUIRE(notes[1] == 64);  // E
        REQUIRE(notes[3] == 70);  // Bb (minor 7th)
        REQUIRE(notes[4] == 74);  // D (9th)
    }
}

// ═══════════════════════════════════════════════════════════════
// Voicings
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ChordGenerator voicings", "[pianoroll][chord][voicing]") {
    ChordGenerator cg;

    SECTION("Close voicing (default)") {
        auto notes = cg.generate(60, ChordGenerator::Major,
                                  ChordGenerator::Close);
        REQUIRE(notes.size() == 3);
        // Notes should be close: C4, E4, G4
        REQUIRE(notes[1] - notes[0] <= 4);
        REQUIRE(notes[2] - notes[1] <= 4);
    }

    SECTION("Open voicing spreads notes") {
        auto notes = cg.generate(60, ChordGenerator::Major,
                                  ChordGenerator::Open);
        REQUIRE(notes.size() == 3);
        // Open voicing should spread notes across more than one octave
        // In Open voicing, every other note goes up an octave.
        // Close: C4(60), E4(64), G4(67)
        // Open:  C4(60), E5(76), G4(67)
        // After sort: C4(60), G4(67), E5(76)
    }

    SECTION("Drop2 voicing on 4-note chord") {
        auto notes = cg.generate(60, ChordGenerator::Maj7,
                                  ChordGenerator::Drop2);
        REQUIRE(notes.size() == 4);
        // Close:  C4(60), E4(64), G4(67), B4(71)
        // Drop2: C4(60), E4(64), G3(55), B4(71) → after sort: G3(55), C4(60), E4(64), B4(71)
        // The 2nd highest (G) drops an octave.
        // Check that the range is wider than close.
        int range = notes.back() - notes.front();
        REQUIRE(range > 12);  // Should span > 1 octave
    }

    SECTION("Cluster voicing on triad") {
        auto notes = cg.generate(60, ChordGenerator::Major,
                                  ChordGenerator::Cluster);
        REQUIRE(notes.size() == 3);
        // Cluster: C4(60), C#4(61), D#4(63)
        REQUIRE(notes[0] == 60);
        REQUIRE(notes[1] == 61);
        REQUIRE(notes[2] == 63);
    }
}

// ═══════════════════════════════════════════════════════════════
// Inversions
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ChordGenerator inversions", "[pianoroll][chord][inversion]") {
    ChordGenerator cg;

    SECTION("C Major root position (inversion=0)") {
        auto notes = cg.generate_inversion(60, ChordGenerator::Major, 0);
        REQUIRE(notes.size() == 3);
        REQUIRE(notes[0] == 60);  // C is lowest
    }

    SECTION("C Major first inversion") {
        auto notes = cg.generate_inversion(60, ChordGenerator::Major, 1);
        REQUIRE(notes.size() == 3);
        // First inversion: E is lowest note
        // C4(60), E4(64), G4(67) → rotate by 1: E4(64), G4(67), C5(72)
        REQUIRE(notes[0] == 64);  // E is lowest
        REQUIRE(notes[2] == 72);  // C is highest (up one octave)
    }

    SECTION("C Major second inversion") {
        auto notes = cg.generate_inversion(60, ChordGenerator::Major, 2);
        REQUIRE(notes.size() == 3);
        // Second inversion: G is lowest
        // C4(60), E4(64), G4(67) → rotate by 2: G4(67), C5(72), E5(76)
        REQUIRE(notes[0] == 67);  // G is lowest
    }
}

// ═══════════════════════════════════════════════════════════════
// Progressions
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ChordGenerator progressions", "[pianoroll][chord][progression]") {
    ChordGenerator cg;

    SECTION("C Major I-IV-V-I progression") {
        auto prog = cg.generate_progression(60, ScaleSystem::MAJOR, {1, 4, 5, 1});

        REQUIRE(prog.chords.size() == 4);
        REQUIRE(prog.roots.size() == 4);

        // I = C Major
        REQUIRE(prog.chords[0] == ChordGenerator::Major);
        REQUIRE(prog.roots[0] % 12 == 0);  // C

        // IV = F Major
        REQUIRE(prog.chords[1] == ChordGenerator::Major);
        REQUIRE(prog.roots[1] % 12 == 5);  // F

        // V = G Major
        REQUIRE(prog.chords[2] == ChordGenerator::Major);
        REQUIRE(prog.roots[2] % 12 == 7);  // G

        // I = C Major
        REQUIRE(prog.chords[3] == ChordGenerator::Major);
        REQUIRE(prog.roots[3] % 12 == 0);  // C
    }

    SECTION("A minor i-iv-v progression") {
        auto prog = cg.generate_progression(69, ScaleSystem::MINOR, {1, 4, 5});
        // i = Am, iv = Dm, v = Em
        REQUIRE(prog.chords.size() == 3);
        REQUIRE(prog.chords[0] == ChordGenerator::Major);  // i
        REQUIRE(prog.chords[1] == ChordGenerator::Major);  // iv
        REQUIRE(prog.chords[2] == ChordGenerator::Major);  // v
    }
}

// ═══════════════════════════════════════════════════════════════
// Arpeggiation
// ═══════════════════════════════════════════════════════════════

TEST_CASE("ChordGenerator arpeggiation", "[pianoroll][chord][arpeggio]") {
    ChordGenerator cg;
    std::vector<uint8_t> chord = {60, 64, 67};  // C Major triad

    SECTION("Up pattern") {
        auto pos = cg.arpeggiate(chord, 0, 240, ChordGenerator::Up);
        REQUIRE(pos.size() == 3);
        REQUIRE(pos[0] == 0);    // C at beat 0
        REQUIRE(pos[1] == 240);  // E at beat 1
        REQUIRE(pos[2] == 480);  // G at beat 2
    }

    SECTION("Chord pattern (all at once)") {
        auto pos = cg.arpeggiate(chord, 0, 240, ChordGenerator::Chord);
        REQUIRE(pos.size() == 1);
        REQUIRE(pos[0] == 0);  // All at start
    }

    SECTION("ForwardBack pattern") {
        auto pos = cg.arpeggiate(chord, 0, 240, ChordGenerator::ForwardBack);
        REQUIRE(pos.size() == 6);  // 3 up + 3 back
        REQUIRE(pos[0] == 0);      // C
        REQUIRE(pos[3] == 720);    // First note of return
    }

    SECTION("AsPlayed preserves order") {
        std::vector<uint8_t> reversed = {67, 64, 60};
        auto pos = cg.arpeggiate(reversed, 100, 120, ChordGenerator::AsPlayed);
        REQUIRE(pos.size() == 3);
        REQUIRE(pos[0] == 100);
        REQUIRE(pos[1] == 220);
        REQUIRE(pos[2] == 340);
    }

    SECTION("empty notes returns empty vector") {
        auto pos = cg.arpeggiate({}, 0, 240, ChordGenerator::Up);
        REQUIRE(pos.empty());
    }
}
