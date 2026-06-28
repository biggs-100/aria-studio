#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "timeline/timeline.h"
#include "timeline/tempo_track.h"
#include "audio/tempo_map.h"

using namespace aria;

// ─── PPQN ↔ Bar/Beat conversion ───────────────────────────────

TEST_CASE("Timeline — ppqn_to_bar_beat default 4/4", "[timeline]") {
    Timeline tl;

    SECTION("PPQN 0 → bar 1, beat 1, sixteenth 0") {
        auto bb = tl.ppqn_to_bar_beat(0);
        REQUIRE(bb.bar == 1);
        REQUIRE(bb.beat == 1);
        REQUIRE(bb.sixteenth == 0);
        REQUIRE(bb.ticks == Catch::Approx(0.0));
    }

    SECTION("PPQN 960 → bar 1, beat 2") {
        auto bb = tl.ppqn_to_bar_beat(960);
        REQUIRE(bb.bar == 1);
        REQUIRE(bb.beat == 2);
    }

    SECTION("PPQN 3840 → bar 2, beat 1") {
        auto bb = tl.ppqn_to_bar_beat(3840);
        REQUIRE(bb.bar == 2);
        REQUIRE(bb.beat == 1);
    }

    SECTION("PPQN 240 → sixteenth 1") {
        auto bb = tl.ppqn_to_bar_beat(240);
        REQUIRE(bb.bar == 1);
        REQUIRE(bb.beat == 1);
        REQUIRE(bb.sixteenth == 1);
    }

    SECTION("PPQN 9600 → bar 3, beat 3") {
        auto bb = tl.ppqn_to_bar_beat(9600);
        REQUIRE(bb.bar == 3);
        REQUIRE(bb.beat == 3);  // 9600-7680=1920, 1920/960=2 → 0-based beat 2 → 1-based beat 3
    }
}

TEST_CASE("Timeline — bar_beat_to_ppqn default 4/4", "[timeline]") {
    Timeline tl;

    SECTION("bar 1, beat 1, sixteenth 0 → 0") {
        REQUIRE(tl.bar_beat_to_ppqn(1, 1, 0) == 0);
    }

    SECTION("bar 1, beat 2 → 960") {
        REQUIRE(tl.bar_beat_to_ppqn(1, 2, 0) == 960);
    }

    SECTION("bar 2, beat 1 → 3840") {
        REQUIRE(tl.bar_beat_to_ppqn(2, 1, 0) == 3840);
    }

    SECTION("bar 1, beat 1, sixteenth 1 → 240") {
        REQUIRE(tl.bar_beat_to_ppqn(1, 1, 1) == 240);
    }

    SECTION("round-trip consistency") {
        for (uint64_t ppqn : {0ULL, 240ULL, 960ULL, 3840ULL, 9600ULL, 12345ULL}) {
            auto bb = tl.ppqn_to_bar_beat(ppqn);
            uint64_t back = tl.bar_beat_to_ppqn(bb.bar, bb.beat, bb.sixteenth);
            // Lossy at sub-sixteenth level, but beat+bar should round-trip
            REQUIRE(back <= ppqn + 239);  // within one sixteenth
        }
    }
}

TEST_CASE("Timeline — time signature 3/4", "[timeline]") {
    Timeline tl;
    tl.set_time_signature(3, 4);

    SECTION("3/4: 1 bar = 3 beats = 2880 PPQN") {
        auto bb = tl.ppqn_to_bar_beat(0);
        REQUIRE(bb.bar == 1);
        REQUIRE(bb.beat == 1);

        bb = tl.ppqn_to_bar_beat(960);
        REQUIRE(bb.bar == 1);
        REQUIRE(bb.beat == 2);

        bb = tl.ppqn_to_bar_beat(2880);
        REQUIRE(bb.bar == 2);
        REQUIRE(bb.beat == 1);
    }

    SECTION("3/4: bar_beat_to_ppqn") {
        REQUIRE(tl.bar_beat_to_ppqn(2, 1, 0) == 2880);
    }
}

TEST_CASE("Timeline — time signature 6/8", "[timeline]") {
    Timeline tl;
    tl.set_time_signature(6, 8);

    // 6/8: 6 beats per bar, 1 beat = 480 PPQN (= 960 * 4 / 8)
    SECTION("6/8: 1 bar = 6 beats = 2880 PPQN") {
        auto bb = tl.ppqn_to_bar_beat(0);
        REQUIRE(bb.bar == 1);
        REQUIRE(bb.beat == 1);

        bb = tl.ppqn_to_bar_beat(480);
        REQUIRE(bb.bar == 1);
        REQUIRE(bb.beat == 2);

        bb = tl.ppqn_to_bar_beat(2880);
        REQUIRE(bb.bar == 2);
        REQUIRE(bb.beat == 1);
    }

    REQUIRE(tl.bar_beat_to_ppqn(2, 1, 0) == 2880);
}

// ─── PPQN ↔ Seconds conversion ────────────────────────────────

TEST_CASE("Timeline — ppqn_to_seconds at 120 BPM", "[timeline]") {
    Timeline tl;
    TempoMap map;
    map.set_default_bpm(120.0);

    // 1 beat = 960 PPQN = 0.5 seconds at 120 BPM
    SECTION("960 PPQN → 0.5s") {
        double sec = tl.ppqn_to_seconds(960, map);
        REQUIRE(sec == Catch::Approx(0.5));
    }

    // 4 beats = 3840 PPQN = 2.0 seconds
    SECTION("3840 PPQN → 2.0s") {
        double sec = tl.ppqn_to_seconds(3840, map);
        REQUIRE(sec == Catch::Approx(2.0));
    }

    // 0 PPQN → 0.0s
    SECTION("0 PPQN → 0.0s") {
        REQUIRE(tl.ppqn_to_seconds(0, map) == Catch::Approx(0.0));
    }
}

TEST_CASE("Timeline — seconds_to_ppqn at 120 BPM", "[timeline]") {
    Timeline tl;
    TempoMap map;
    map.set_default_bpm(120.0);

    SECTION("0.5s → 960 PPQN") {
        REQUIRE(tl.seconds_to_ppqn(0.5, map) == 960);
    }

    SECTION("2.0s → 3840 PPQN") {
        REQUIRE(tl.seconds_to_ppqn(2.0, map) == 3840);
    }

    SECTION("0.0s → 0") {
        REQUIRE(tl.seconds_to_ppqn(0.0, map) == 0);
    }
}

TEST_CASE("Timeline — conversion at different BPMs", "[timeline]") {
    Timeline tl;
    TempoMap map;

    SECTION("140 BPM: 960 PPQN → ~0.4286s") {
        map.set_default_bpm(140.0);
        double sec = tl.ppqn_to_seconds(960, map);
        REQUIRE(sec == Catch::Approx(0.428571).margin(0.001));
    }

    SECTION("80 BPM: 2.0s → 2560 PPQN") {
        map.set_default_bpm(80.0);
        uint64_t ppqn = tl.seconds_to_ppqn(2.0, map);
        REQUIRE(ppqn == 2560);  // 2s * (80/60) * 960 = 2560
    }
}

// ─── TempoTrack ───────────────────────────────────────────────

TEST_CASE("TempoTrack — BPM queries", "[timeline][tempo]") {
    TempoTrack tt;

    SECTION("no events → default 120 BPM") {
        REQUIRE(tt.bpm_at(0) == Catch::Approx(120.0));
        REQUIRE(tt.bpm_at(960) == Catch::Approx(120.0));
    }

    SECTION("custom default") {
        tt.set_default_bpm(140.0);
        REQUIRE(tt.bpm_at(0) == Catch::Approx(140.0));
    }

    SECTION("single tempo event at PPQN 0") {
        tt.add_tempo_event(0, 130.0);
        REQUIRE(tt.bpm_at(0) == Catch::Approx(130.0));
        REQUIRE(tt.bpm_at(960) == Catch::Approx(130.0));
    }

    SECTION("tempo change at PPQN 960") {
        tt.add_tempo_event(0, 120.0);
        tt.add_tempo_event(960, 140.0);

        REQUIRE(tt.bpm_at(0) == Catch::Approx(120.0));
        REQUIRE(tt.bpm_at(480) == Catch::Approx(120.0));
        REQUIRE(tt.bpm_at(960) == Catch::Approx(140.0));
        REQUIRE(tt.bpm_at(1920) == Catch::Approx(140.0));
    }

    SECTION("replace existing event at same PPQN") {
        tt.add_tempo_event(0, 120.0);
        tt.add_tempo_event(0, 130.0);  // same position → replace
        REQUIRE(tt.bpm_at(0) == Catch::Approx(130.0));
        REQUIRE(tt.event_count() == 1);
    }
}

TEST_CASE("TempoTrack — time signature queries", "[timeline][tempo]") {
    TempoTrack tt;

    SECTION("no events → default 4/4") {
        auto ts = tt.time_sig_at(0);
        REQUIRE(ts.numerator == 4);
        REQUIRE(ts.denominator == 4);
    }

    SECTION("single event") {
        tt.add_time_sig_event(0, 3, 4);
        auto ts = tt.time_sig_at(0);
        REQUIRE(ts.numerator == 3);
        REQUIRE(ts.denominator == 4);
    }

    SECTION("time sig change") {
        tt.add_time_sig_event(0, 4, 4);
        tt.add_time_sig_event(3840, 3, 4);  // bar 2

        auto ts = tt.time_sig_at(0);
        REQUIRE(ts.numerator == 4);

        ts = tt.time_sig_at(3839);
        REQUIRE(ts.numerator == 4);

        ts = tt.time_sig_at(3840);
        REQUIRE(ts.numerator == 3);
    }

    SECTION("event count") {
        REQUIRE(tt.time_sig_count() == 0);
        tt.add_time_sig_event(0, 4, 4);
        REQUIRE(tt.time_sig_count() == 1);
    }
}
