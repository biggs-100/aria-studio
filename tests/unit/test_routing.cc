#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "mixer/channel.h"
#include "mixer/routing.h"
#include "mixer/sidechain.h"

#include <cmath>

using namespace aria;

// ─── RouteTarget ───────────────────────────────────────────────

TEST_CASE("RouteTarget construction", "[mixer][routing]") {
    RouteTarget rt;

    SECTION("default target type is Master") {
        REQUIRE(rt.type == RouteTarget::Master);
    }

    SECTION("default format is Stereo") {
        REQUIRE(rt.format == RouteTarget::Format::Stereo);
    }

    SECTION("can be set to Bus target") {
        rt.type = RouteTarget::Bus;
        rt.bus_id = 42;
        REQUIRE(rt.type == RouteTarget::Bus);
        REQUIRE(rt.bus_id == 42);
    }

    SECTION("can be set to Track target") {
        rt.type = RouteTarget::Track;
        rt.track_id = 100;
        REQUIRE(rt.type == RouteTarget::Track);
        REQUIRE(rt.track_id == 100);
    }

    SECTION("can be set to External target") {
        rt.type = RouteTarget::External;
        rt.external_channel = 3;
        REQUIRE(rt.type == RouteTarget::External);
        REQUIRE(rt.external_channel == 3);
    }

    SECTION("can be set to None") {
        rt.type = RouteTarget::None;
        REQUIRE(rt.type == RouteTarget::None);
    }

    SECTION("format can be Mono") {
        rt.format = RouteTarget::Format::Mono;
        REQUIRE(rt.format == RouteTarget::Format::Mono);
    }

    SECTION("format can be DualMono") {
        rt.format = RouteTarget::Format::DualMono;
        REQUIRE(rt.format == RouteTarget::Format::DualMono);
    }
}

// ─── AudioInput ────────────────────────────────────────────────

TEST_CASE("AudioInput construction", "[mixer][routing]") {
    AudioInput ai;

    SECTION("default input type is None") {
        REQUIRE(ai.type == AudioInput::None);
    }

    SECTION("default channel is 0") {
        REQUIRE(ai.channel == 0);
    }

    SECTION("default device_id is empty") {
        REQUIRE(ai.device_id.empty());
    }

    SECTION("can be set to Internal") {
        ai.type = AudioInput::Internal;
        ai.channel = 1;
        REQUIRE(ai.type == AudioInput::Internal);
        REQUIRE(ai.channel == 1);
    }

    SECTION("can be set to ExternalAudio") {
        ai.type = AudioInput::ExternalAudio;
        ai.device_id = "ASIO:Focusrite";
        REQUIRE(ai.type == AudioInput::ExternalAudio);
        REQUIRE(ai.device_id == "ASIO:Focusrite");
    }

    SECTION("can be set to ExternalMIDI") {
        ai.type = AudioInput::ExternalMIDI;
        REQUIRE(ai.type == AudioInput::ExternalMIDI);
    }

    SECTION("can be set to Sidechain") {
        ai.type = AudioInput::Sidechain;
        REQUIRE(ai.type == AudioInput::Sidechain);
    }
}

// ─── SendSlot (from Channel) ───────────────────────────────────

TEST_CASE("SendSlot construction", "[mixer][routing]") {
    SendSlot slot;

    SECTION("default target is 0") {
        REQUIRE(slot.target == 0);
    }

    SECTION("default level is -60 dB") {
        REQUIRE(slot.level_db == Catch::Approx(-60.0));
    }

    SECTION("default is pre-fader") {
        REQUIRE(slot.pre_fader == true);
    }

    SECTION("default is post-FX") {
        REQUIRE(slot.pre_fx == false);
    }

    SECTION("custom SendSlot") {
        SendSlot custom;
        custom.target    = 5;
        custom.level_db  = -12.0;
        custom.pre_fader = false;
        custom.pre_fx    = true;

        REQUIRE(custom.target    == 5);
        REQUIRE(custom.level_db  == Catch::Approx(-12.0));
        REQUIRE(custom.pre_fader == false);
        REQUIRE(custom.pre_fx    == true);
    }
}

// ─── Channel Sends (integrated with Channel) ──────────────────

TEST_CASE("Channel sends integration", "[mixer][routing][sends]") {
    Channel source(1, "Bass", ChannelType::Audio);
    Channel target(2, "Reverb Return", ChannelType::Return);

    SECTION("send from source to target channel") {
        SendID id = source.add_send(target.id(), -12.0, true);
        REQUIRE(id != kInvalidSendID);

        auto sends = source.sends();
        REQUIRE(sends.size() == 1);
        REQUIRE(sends[0].target == target.id());
        REQUIRE(sends[0].level_db == Catch::Approx(-12.0));
        REQUIRE(sends[0].pre_fader == true);
    }

    SECTION("multiple sends from same source") {
        SendID id1 = source.add_send(2, -12.0, true);
        SendID id2 = source.add_send(3, -6.0, false);
        REQUIRE(id1 != id2);

        auto sends = source.sends();
        REQUIRE(sends.size() == 2);
    }

    SECTION("remove one send keeps others") {
        SendID id1 = source.add_send(2, -12.0, true);
        source.add_send(3, -6.0, false);
        source.remove_send(id1);

        auto sends = source.sends();
        REQUIRE(sends.size() == 1);
        REQUIRE(sends[0].target == 3);
    }
}

// ─── SidechainManager ──────────────────────────────────────────

TEST_CASE("SidechainManager basic operations", "[mixer][sidechain]") {
    SidechainManager sc;

    SECTION("no sidechain by default") {
        REQUIRE_FALSE(sc.has_sidechain(1));
    }

    SECTION("set_sidechain creates connection") {
        sc.set_sidechain(1, 2);
        REQUIRE(sc.has_sidechain(2));
    }

    SECTION("get_sidechain_buffer returns nullptr for unknown") {
        REQUIRE(sc.get_sidechain_buffer(99) == nullptr);
    }

    SECTION("get_sidechain_buffer returns buffer after set") {
        sc.set_sidechain(1, 2);
        const auto* buf = sc.get_sidechain_buffer(2);
        REQUIRE(buf != nullptr);
    }

    SECTION("remove_sidechain removes connection") {
        sc.set_sidechain(1, 2);
        REQUIRE(sc.has_sidechain(2));
        sc.remove_sidechain(2);
        REQUIRE_FALSE(sc.has_sidechain(2));
    }

    SECTION("multiple sidechain connections") {
        sc.set_sidechain(1, 2);
        sc.set_sidechain(3, 4);
        REQUIRE(sc.has_sidechain(2));
        REQUIRE(sc.has_sidechain(4));
    }

    SECTION("remove_sidechain only removes specified target") {
        sc.set_sidechain(1, 2);
        sc.set_sidechain(3, 4);
        sc.remove_sidechain(2);
        REQUIRE_FALSE(sc.has_sidechain(2));
        REQUIRE(sc.has_sidechain(4));
    }
}

TEST_CASE("SidechainManager prepare and process", "[mixer][sidechain]") {
    SidechainManager sc;

    SECTION("prepare allocates buffers") {
        sc.set_sidechain(1, 2);
        sc.prepare(2, 256);
        const auto* buf = sc.get_sidechain_buffer(2);
        REQUIRE(buf != nullptr);
        REQUIRE(buf->channels == 2);
        REQUIRE(buf->capacity >= 256);
    }

    SECTION("process does not crash") {
        sc.set_sidechain(1, 2);
        sc.prepare(2, 64);
        sc.process(64);
        // Process is a placeholder — should not crash
    }
}
