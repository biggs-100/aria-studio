#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mixer/channel.h"

#include <cmath>

using namespace aria;

// ─── Channel Construction ───────────────────────────────────────

TEST_CASE("Channel construction and identity", "[mixer][channel]") {
    Channel ch(42, "Kick", ChannelType::Audio);

    SECTION("id returns assigned ID") {
        REQUIRE(ch.id() == 42);
    }

    SECTION("name returns assigned name") {
        REQUIRE(ch.name() == "Kick");
    }

    SECTION("type returns assigned type") {
        REQUIRE(ch.type() == ChannelType::Audio);
    }

    SECTION("set_name updates name") {
        ch.set_name("Snare");
        REQUIRE(ch.name() == "Snare");
    }

    SECTION("channel types can be set") {
        Channel midi_ch(1, "MIDI", ChannelType::MIDI);
        REQUIRE(midi_ch.type() == ChannelType::MIDI);

        Channel grp_ch(2, "Group", ChannelType::Group);
        REQUIRE(grp_ch.type() == ChannelType::Group);

        Channel ret_ch(3, "Reverb", ChannelType::Return);
        REQUIRE(ret_ch.type() == ChannelType::Return);

        Channel vca_ch(4, "VCA", ChannelType::VCA);
        REQUIRE(vca_ch.type() == ChannelType::VCA);

        Channel mst_ch(5, "Master", ChannelType::Master);
        REQUIRE(mst_ch.type() == ChannelType::Master);
    }
}

// ─── Volume ─────────────────────────────────────────────────────

TEST_CASE("Channel volume control", "[mixer][channel][volume]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("default volume is 0 dB (unity)") {
        REQUIRE(ch.volume() == Catch::Approx(0.0));
    }

    SECTION("set_volume clamps to +24 dB max") {
        ch.set_volume(30.0);
        REQUIRE(ch.volume() == Catch::Approx(24.0));
    }

    SECTION("set_volume clamps to -120 dB min") {
        ch.set_volume(-200.0);
        REQUIRE(ch.volume() == Catch::Approx(-120.0));
    }

    SECTION("set_volume accepts normal range") {
        ch.set_volume(-6.0);
        REQUIRE(ch.volume() == Catch::Approx(-6.0));
    }

    SECTION("linear_volume at 0 dB is 1.0") {
        ch.set_volume(0.0);
        REQUIRE(ch.linear_volume() == Catch::Approx(1.0));
    }

    SECTION("linear_volume at -6 dB is ~0.5") {
        ch.set_volume(-6.0);
        REQUIRE(ch.linear_volume() == Catch::Approx(0.501187).margin(0.001));
    }

    SECTION("linear_volume at -120 dB is 0.0") {
        ch.set_volume(-120.0);
        REQUIRE(ch.linear_volume() == Catch::Approx(0.0));
    }

    SECTION("linear_volume at +6 dB is ~2.0") {
        ch.set_volume(6.0);
        REQUIRE(ch.linear_volume() == Catch::Approx(1.99526).margin(0.001));
    }
}

// ─── Pan ────────────────────────────────────────────────────────

TEST_CASE("Channel pan control", "[mixer][channel][pan]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("default pan is 0 (center)") {
        REQUIRE(ch.pan() == Catch::Approx(0.0));
    }

    SECTION("set_pan accepts -1 (full left)") {
        ch.set_pan(-1.0);
        REQUIRE(ch.pan() == Catch::Approx(-1.0));
    }

    SECTION("set_pan accepts +1 (full right)") {
        ch.set_pan(1.0);
        REQUIRE(ch.pan() == Catch::Approx(1.0));
    }

    SECTION("set_pan clamps below -1") {
        ch.set_pan(-2.0);
        REQUIRE(ch.pan() == Catch::Approx(-1.0));
    }

    SECTION("set_pan clamps above +1") {
        ch.set_pan(2.0);
        REQUIRE(ch.pan() == Catch::Approx(1.0));
    }

    SECTION("set_pan accepts intermediate values") {
        ch.set_pan(-0.5);
        REQUIRE(ch.pan() == Catch::Approx(-0.5));
    }
}

// ─── Mute & Solo ───────────────────────────────────────────────

TEST_CASE("Channel mute and solo", "[mixer][channel][mute][solo]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("default is not muted") {
        REQUIRE_FALSE(ch.is_muted());
    }

    SECTION("set_mute(true) mutes channel") {
        ch.set_mute(true);
        REQUIRE(ch.is_muted());
    }

    SECTION("set_mute(false) unmutes channel") {
        ch.set_mute(true);
        ch.set_mute(false);
        REQUIRE_FALSE(ch.is_muted());
    }

    SECTION("default is not soloed") {
        REQUIRE_FALSE(ch.is_soloed());
    }

    SECTION("set_solo(true) solos channel") {
        ch.set_solo(true);
        REQUIRE(ch.is_soloed());
    }

    SECTION("mute and solo are independent") {
        ch.set_mute(true);
        ch.set_solo(true);
        REQUIRE(ch.is_muted());
        REQUIRE(ch.is_soloed());
    }
}

// ─── Phase Invert ──────────────────────────────────────────────

TEST_CASE("Channel phase invert", "[mixer][channel][phase]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("default is not inverted") {
        REQUIRE_FALSE(ch.phase_inverted());
    }

    SECTION("set_phase_invert(true) inverts phase") {
        ch.set_phase_invert(true);
        REQUIRE(ch.phase_inverted());
    }

    SECTION("set_phase_invert(false) reverts") {
        ch.set_phase_invert(true);
        ch.set_phase_invert(false);
        REQUIRE_FALSE(ch.phase_inverted());
    }
}

// ─── Width ─────────────────────────────────────────────────────

TEST_CASE("Channel width control", "[mixer][channel][width]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("default width is 1.0") {
        REQUIRE(ch.width() == Catch::Approx(1.0));
    }

    SECTION("set_width accepts 0.0 (mono)") {
        ch.set_width(0.0);
        REQUIRE(ch.width() == Catch::Approx(0.0));
    }

    SECTION("set_width accepts 2.0 (wide)") {
        ch.set_width(2.0);
        REQUIRE(ch.width() == Catch::Approx(2.0));
    }

    SECTION("set_width clamps negative") {
        ch.set_width(-1.0);
        REQUIRE(ch.width() == Catch::Approx(0.0));
    }

    SECTION("set_width clamps above 2.0") {
        ch.set_width(3.0);
        REQUIRE(ch.width() == Catch::Approx(2.0));
    }
}

// ─── Automation ────────────────────────────────────────────────

TEST_CASE("Channel automation override", "[mixer][channel][automation]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("effective_volume equals volume with no automation") {
        ch.set_volume(-6.0);
        REQUIRE(ch.effective_volume() == Catch::Approx(-6.0));
    }

    SECTION("effective_volume includes automation offset") {
        ch.set_volume(0.0);
        ch.set_automated_volume(-3.0);
        REQUIRE(ch.effective_volume() == Catch::Approx(-3.0));
    }

    SECTION("effective_volume combines all contributions") {
        ch.set_volume(-6.0);
        ch.set_automated_volume(2.0);
        // VCA contribution is 0 by default internally
        REQUIRE(ch.effective_volume() == Catch::Approx(-4.0));
    }
}

// ─── Groups & VCA ──────────────────────────────────────────────

TEST_CASE("Channel groups and VCA", "[mixer][channel][group][vca]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("default group is invalid") {
        REQUIRE(ch.group() == kInvalidBusID);
    }

    SECTION("set_group assigns bus") {
        ch.set_group(42);
        REQUIRE(ch.group() == 42);
    }

    SECTION("default VCA is invalid") {
        REQUIRE(ch.vca() == kInvalidTrackID);
    }

    SECTION("set_vca assigns VCA track") {
        ch.set_vca(100);
        REQUIRE(ch.vca() == 100);
    }
}

// ─── FX Chain ─────────────────────────────────────────────────

TEST_CASE("Channel FX chain", "[mixer][channel][fx]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("default FX count is 0") {
        REQUIRE(ch.fx_count() == 0);
    }

    SECTION("add_fx adds a plugin") {
        ch.add_fx(1001, 0);
        REQUIRE(ch.fx_count() == 1);
    }

    SECTION("add_fx at high position appends") {
        ch.add_fx(1001, 0);
        ch.add_fx(1002, 10);
        REQUIRE(ch.fx_count() == 2);
    }

    SECTION("remove_fx removes by index") {
        ch.add_fx(1001, 0);
        ch.add_fx(1002, 0);
        ch.remove_fx(0);
        REQUIRE(ch.fx_count() == 1);
    }

    SECTION("remove_fx with invalid index does nothing") {
        ch.remove_fx(99);
        REQUIRE(ch.fx_count() == 0);
    }

    SECTION("set_fx_bypass with invalid index does nothing") {
        // Should not crash
        ch.set_fx_bypass(0, true);
    }
}

// ─── Sends ─────────────────────────────────────────────────────

TEST_CASE("Channel sends", "[mixer][channel][sends]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("default sends is empty") {
        REQUIRE(ch.sends().empty());
    }

    SECTION("add_send creates a send and returns ID") {
        SendID id = ch.add_send(2, -12.0, true);
        REQUIRE(id != kInvalidSendID);

        auto sends = ch.sends();
        REQUIRE(sends.size() == 1);
        REQUIRE(sends[0].target == 2);
        REQUIRE(sends[0].level_db == Catch::Approx(-12.0));
        REQUIRE(sends[0].pre_fader == true);
    }

    SECTION("add_send allows pre-fader false") {
        ch.add_send(3, -6.0, false);
        auto sends = ch.sends();
        REQUIRE(sends[0].pre_fader == false);
    }

    SECTION("remove_send removes by ID") {
        SendID id = ch.add_send(2, -12.0, true);
        REQUIRE(ch.sends().size() == 1);
        ch.remove_send(id);
        REQUIRE(ch.sends().empty());
    }

    SECTION("remove_send with invalid ID does nothing") {
        ch.add_send(2, -12.0, true);
        ch.remove_send(999);
        REQUIRE(ch.sends().size() == 1);
    }

    SECTION("set_send_level updates level") {
        SendID id = ch.add_send(2, -12.0, true);
        ch.set_send_level(id, -6.0);
        auto sends = ch.sends();
        REQUIRE(sends[0].level_db == Catch::Approx(-6.0));
    }
}

// ─── EQ ────────────────────────────────────────────────────────

TEST_CASE("Channel built-in EQ", "[mixer][channel][eq]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("eq() returns accessible reference") {
        auto& eq = ch.eq();
        REQUIRE_FALSE(eq.is_bypassed());
    }

    SECTION("EQ bypass can be toggled") {
        auto& eq = ch.eq();
        eq.set_bypass(true);
        REQUIRE(eq.is_bypassed());
        eq.set_bypass(false);
        REQUIRE_FALSE(eq.is_bypassed());
    }

    SECTION("EQ latency is always 0") {
        REQUIRE(ch.eq().latency() == 0);
    }
}

// ─── Metering ──────────────────────────────────────────────────

TEST_CASE("Channel metering defaults", "[mixer][channel][metering]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("peak values start at -60 dB") {
        REQUIRE(ch.peak_left()  == Catch::Approx(-60.0f));
        REQUIRE(ch.peak_right() == Catch::Approx(-60.0f));
    }

    SECTION("rms values start at -60 dB") {
        REQUIRE(ch.rms_left()  == Catch::Approx(-60.0f));
        REQUIRE(ch.rms_right() == Catch::Approx(-60.0f));
    }
}

// ─── Utility Functions ─────────────────────────────────────────

TEST_CASE("db_to_linear utility", "[mixer][utility]") {
    SECTION("0 dB → 1.0") {
        REQUIRE(db_to_linear(0.0) == Catch::Approx(1.0));
    }

    SECTION("-6 dB → ~0.5") {
        REQUIRE(db_to_linear(-6.0) == Catch::Approx(0.501187).margin(0.001));
    }

    SECTION("-120 dB → 0.0") {
        REQUIRE(db_to_linear(-120.0) == Catch::Approx(0.0));
    }

    SECTION("below -120 dB → 0.0") {
        REQUIRE(db_to_linear(-200.0) == Catch::Approx(0.0));
    }

    SECTION("+6 dB → ~2.0") {
        REQUIRE(db_to_linear(6.0) == Catch::Approx(1.99526).margin(0.001));
    }
}

TEST_CASE("linear_to_db utility", "[mixer][utility]") {
    SECTION("1.0 → 0 dB") {
        REQUIRE(linear_to_db(1.0) == Catch::Approx(0.0));
    }

    SECTION("0.5 → ~-6 dB") {
        REQUIRE(linear_to_db(0.5) == Catch::Approx(-6.0206).margin(0.01));
    }

    SECTION("0.0 → -120 dB") {
        REQUIRE(linear_to_db(0.0) == Catch::Approx(-120.0));
    }
}

// ─── Input Index ───────────────────────────────────────────────

TEST_CASE("Channel input index", "[mixer][channel]") {
    Channel ch(1, "Test", ChannelType::Audio);

    SECTION("default input index is 0") {
        REQUIRE(ch.input_index() == 0);
    }

    SECTION("set_input_index sets the index") {
        ch.set_input_index(5);
        REQUIRE(ch.input_index() == 5);
    }
}
