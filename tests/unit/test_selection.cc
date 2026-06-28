#include <catch2/catch_test_macros.hpp>
#include "pianoroll/note_collection.h"
#include "pianoroll/selection_manager.h"
#include "pianoroll/piano_roll_canvas.h"  // For ViewTransform

using namespace aria;

// ─── Helpers ─────────────────────────────────────────────────────

static Note make_note(uint64_t start, uint64_t dur, uint8_t key,
                       uint8_t vel = 100)
{
    Note n;
    n.start_ppqn    = start;
    n.duration_ppqn = dur;
    n.key           = key;
    n.velocity      = vel;
    return n;
}

static ViewTransform default_view() {
    ViewTransform v;
    v.ppqn_per_pixel_x      = 1.0;
    v.pixels_per_semitone_y = 8.0;
    v.scroll_ppqn           = 0.0;
    v.scroll_key            = 0.0;
    return v;
}

// ═══════════════════════════════════════════════════════════════
// Core selection tests
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SelectionManager basic operations", "[pianoroll][selection]") {
    SelectionManager sel;
    NoteCollection notes;

    auto id1 = notes.add(make_note(0, 240, 60));
    auto id2 = notes.add(make_note(240, 240, 64));
    auto id3 = notes.add(make_note(480, 240, 67));

    SECTION("starts with empty selection") {
        REQUIRE(sel.count() == 0);
        REQUIRE(sel.selected().empty());
    }

    SECTION("select_note adds to selection") {
        sel.select_note(id1);
        REQUIRE(sel.count() == 1);
        REQUIRE(sel.is_selected(id1));
        REQUIRE_FALSE(sel.is_selected(id2));
    }

    SECTION("deselect_note removes from selection") {
        sel.select_note(id1);
        sel.deselect_note(id1);
        REQUIRE(sel.count() == 0);
    }

    SECTION("deselect_note on unselected note is a no-op") {
        sel.deselect_note(id1);  // Should not crash
        REQUIRE(sel.count() == 0);
    }

    SECTION("toggle_note adds when not selected") {
        sel.toggle_note(id1);
        REQUIRE(sel.is_selected(id1));
    }

    SECTION("toggle_note removes when selected") {
        sel.select_note(id1);
        sel.toggle_note(id1);
        REQUIRE_FALSE(sel.is_selected(id1));
    }

    SECTION("select_all adds all notes") {
        sel.select_all(notes);
        REQUIRE(sel.count() == 3);
    }

    SECTION("clear removes all") {
        sel.select_note(id1);
        sel.select_note(id2);
        sel.clear();
        REQUIRE(sel.count() == 0);
    }

    SECTION("multiple selection") {
        sel.select_note(id1);
        sel.select_note(id3);
        REQUIRE(sel.count() == 2);
        REQUIRE(sel.is_selected(id1));
        REQUIRE_FALSE(sel.is_selected(id2));
        REQUIRE(sel.is_selected(id3));
    }
}

// ═══════════════════════════════════════════════════════════════
// Marquee / Range selection
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SelectionManager marquee and range", "[pianoroll][selection][marquee]") {
    SelectionManager sel;
    NoteCollection notes;

    auto id1 = notes.add(make_note(0, 240, 60));
    auto id2 = notes.add(make_note(240, 240, 64));
    auto id3 = notes.add(make_note(480, 240, 67));
    auto id4 = notes.add(make_note(960, 240, 72));  // Outside range

    SECTION("select_marquee with PPQN rect selects overlapping notes") {
        // Rect from (10, 58) to (700, 70) — covers id1, id2, id3
        Rect r{10.0, 58.0, 690.0, 12.0};
        sel.select_marquee(r, notes);
        REQUIRE(sel.count() == 3);
        REQUIRE(sel.is_selected(id1));
        REQUIRE(sel.is_selected(id2));
        REQUIRE(sel.is_selected(id3));
        REQUIRE_FALSE(sel.is_selected(id4));
    }

    SECTION("select_range with explicit bounds") {
        sel.select_range(0, 500, 0, 127, notes);
        REQUIRE(sel.count() == 3);  // id1, id2, id3
    }

    SECTION("select_range with narrow key range") {
        sel.select_range(0, 1000, 64, 64, notes);
        // Only id2 has key=64.
        REQUIRE(sel.count() == 1);
        REQUIRE(sel.is_selected(id2));
    }

    SECTION("empty rect selects nothing") {
        Rect r{0, 0, 0, 0};
        sel.select_marquee(r, notes);
        REQUIRE(sel.count() == 0);
    }
}

// ═══════════════════════════════════════════════════════════════
// Handle hit testing
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SelectionManager handle hit testing", "[pianoroll][selection][handles]") {
    SelectionManager sel;
    ViewTransform view = default_view();

    // A note at ppqn=100, duration=240, key=60.
    // Screen pos: x=100, y=480, w=240, h=8
    Note n = make_note(100, 240, 60);

    SECTION("hit test returns None for far away click") {
        auto h = sel.hit_test(n, 0.0f, 0.0f, view);
        REQUIRE(h == SelectionManager::Handle::None);
    }

    SECTION("hit test detects TopLeft corner") {
        auto h = sel.hit_test(n, 100.0f, 480.0f, view);
        REQUIRE(h == SelectionManager::Handle::TopLeft);
    }

    SECTION("hit test detects TopRight corner") {
        // n.start=100, dur=240 → right edge at x=340
        auto h = sel.hit_test(n, 340.0f, 480.0f, view);
        REQUIRE(h == SelectionManager::Handle::TopRight);
    }

    SECTION("hit test detects BottomLeft corner") {
        // n.key=60 → y = 60*8 = 480, bottom = 480+8 = 488
        auto h = sel.hit_test(n, 100.0f, 488.0f, view);
        REQUIRE(h == SelectionManager::Handle::BottomLeft);
    }

    SECTION("hit test detects BottomRight corner") {
        auto h = sel.hit_test(n, 340.0f, 488.0f, view);
        REQUIRE(h == SelectionManager::Handle::BottomRight);
    }

    SECTION("hit test detects Left edge") {
        // Left edge midpoint: x=100, y=484
        auto h = sel.hit_test(n, 100.0f, 484.0f, view);
        REQUIRE(h == SelectionManager::Handle::Left);
    }

    SECTION("hit test detects Right edge") {
        auto h = sel.hit_test(n, 340.0f, 484.0f, view);
        REQUIRE(h == SelectionManager::Handle::Right);
    }

    SECTION("hit test detects Top edge") {
        auto h = sel.hit_test(n, 220.0f, 480.0f, view);
        REQUIRE(h == SelectionManager::Handle::Top);
    }

    SECTION("hit test detects Bottom edge") {
        auto h = sel.hit_test(n, 220.0f, 488.0f, view);
        REQUIRE(h == SelectionManager::Handle::Bottom);
    }
}

// ═══════════════════════════════════════════════════════════════
// Copy / Paste
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SelectionManager copy and paste", "[pianoroll][selection][copy]") {
    SelectionManager sel;
    NoteCollection notes;

    auto id1 = notes.add(make_note(0, 240, 60, 100));
    auto id2 = notes.add(make_note(240, 240, 64, 80));
    auto id3 = notes.add(make_note(480, 240, 67, 120));

    SECTION("copy_selection with empty selection returns empty vector") {
        auto copied = sel.copy_selection(notes);
        REQUIRE(copied.empty());
    }

    SECTION("copy_selection copies selected notes") {
        sel.select_note(id1);
        sel.select_note(id3);
        auto copied = sel.copy_selection(notes);

        REQUIRE(copied.size() == 2);

        // IDs should be cleared for copy.
        REQUIRE(copied[0].id.value == 0);
        REQUIRE(copied[1].id.value == 0);

        // Data should match.
        // Notes may not be in order, but check key values.
        bool found_key_60 = (copied[0].key == 60 || copied[1].key == 60);
        bool found_key_67 = (copied[0].key == 67 || copied[1].key == 67);
        REQUIRE(found_key_60);
        REQUIRE(found_key_67);

        REQUIRE(copied[0].velocity == 100 || copied[0].velocity == 120);
    }
}

// ═══════════════════════════════════════════════════════════════
// Bulk edit
// ═══════════════════════════════════════════════════════════════

TEST_CASE("SelectionManager bulk operations", "[pianoroll][selection][bulk]") {
    SelectionManager sel;
    NoteCollection notes;

    auto id1 = notes.add(make_note(0, 240, 60, 100));
    auto id2 = notes.add(make_note(240, 240, 64, 80));
    auto id3 = notes.add(make_note(480, 240, 67, 120));

    SECTION("move_selection shifts selected notes") {
        sel.select_note(id1);
        sel.select_note(id3);
        sel.move_selection(100, 2, notes);

        REQUIRE(notes.find(id1)->start_ppqn == 100);
        REQUIRE(notes.find(id1)->key == 62);
        // id2 is NOT selected — should NOT move.
        REQUIRE(notes.find(id2)->start_ppqn == 240);
        REQUIRE(notes.find(id2)->key == 64);
        REQUIRE(notes.find(id3)->start_ppqn == 580);
        REQUIRE(notes.find(id3)->key == 69);
    }

    SECTION("move_selection with zero delta does nothing") {
        sel.select_note(id1);
        sel.move_selection(0, 0, notes);
        REQUIRE(notes.find(id1)->start_ppqn == 0);
        REQUIRE(notes.find(id1)->key == 60);
    }

    SECTION("delete_selection removes selected notes") {
        sel.select_note(id1);
        sel.select_note(id2);
        sel.delete_selection(notes);

        REQUIRE(notes.size() == 1);
        REQUIRE(notes.find(id1) == nullptr);
        REQUIRE(notes.find(id2) == nullptr);
        REQUIRE(notes.find(id3) != nullptr);
        REQUIRE(sel.count() == 0);  // Selection should be cleared
    }

    SECTION("duplicate_selection copies selected notes with offset") {
        sel.select_note(id1);

        size_t orig_size = notes.size();
        sel.duplicate_selection(480, notes);

        // Should have one more note.
        REQUIRE(notes.size() == orig_size + 1);

        // The duplicate should be at orig_pos + 480
        // Find the new note (only one with start_ppqn = 480)
        bool found_dup = false;
        for (const auto& n : notes.notes()) {
            if (n.id != id1 && n.key == 60) {
                REQUIRE(n.start_ppqn == 480);
                found_dup = true;
            }
        }
        REQUIRE(found_dup);

        // The new note should be selected (since duplicate selects copies).
        REQUIRE(sel.count() == 1);
    }

    SECTION("multiply_velocity_selection scales velocities") {
        sel.select_note(id1);  // vel=100
        sel.select_note(id2);  // vel=80

        sel.multiply_velocity_selection(0.5, notes);

        REQUIRE(notes.find(id1)->velocity == 50);
        REQUIRE(notes.find(id2)->velocity == 40);
        // id3 is not selected — velocity unchanged.
        REQUIRE(notes.find(id3)->velocity == 120);
    }

    SECTION("transpose_selection shifts selected notes") {
        sel.select_note(id1);
        sel.select_note(id2);

        sel.transpose_selection(-2, notes);

        REQUIRE(notes.find(id1)->key == 58);
        REQUIRE(notes.find(id2)->key == 62);
        // id3 is not selected.
        REQUIRE(notes.find(id3)->key == 67);
    }

    SECTION("transpose_selection clamps to 0-127") {
        Note low = make_note(0, 240, 1);
        auto low_id = notes.add(low);
        sel.select_note(low_id);

        sel.transpose_selection(-5, notes);
        REQUIRE(notes.find(low_id)->key == 0);  // Clamped
    }
}
