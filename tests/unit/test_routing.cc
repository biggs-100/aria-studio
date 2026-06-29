#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "mixer/channel.h"
#include "mixer/routing.h"
#include "mixer/sidechain.h"
#include "audio/audio_buffer.h"

#include <cmath>
#include <cstring>

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
        // Verify data pointers are allocated
        REQUIRE(buf->data[0] != nullptr);
        REQUIRE(buf->data[1] != nullptr);
    }

    SECTION("process copies source audio to target sidechain buffer") {
        // Set up: source=1 → target=2
        sc.set_sidechain(1, 2);
        sc.prepare(2, 64);

        // Create source channel with input_index 0
        Channel source_ch(1, "Kick", ChannelType::Audio);
        source_ch.set_input_index(0);

        // Input buffer with known data
        float src_data[64];
        for (uint32_t i = 0; i < 64; ++i) src_data[i] = 0.5f;

        AudioBuffer src_buf;
        src_buf.frames   = 64;
        src_buf.channels = 1;
        src_buf.capacity = 64;
        src_buf.data[0]  = src_data;

        AudioBuffer* inputs[] = { &src_buf };

        // Process with a lambda that resolves channel IDs
        sc.process(inputs, 1, 64,
            [&source_ch](ChannelID id) -> Channel* {
                return (id == 1) ? &source_ch : nullptr;
            });

        // Verify target's sidechain buffer has the source audio
        const auto* side_buf = sc.get_sidechain_buffer(2);
        REQUIRE(side_buf != nullptr);
        REQUIRE(side_buf->frames >= 64);
        for (uint32_t i = 0; i < 64; ++i) {
            REQUIRE(side_buf->data[0][i] == Catch::Approx(0.5f).margin(0.0001f));
        }
    }

    SECTION("process copies stereo source to sidechain") {
        sc.set_sidechain(1, 2);
        sc.prepare(2, 64);

        Channel source_ch(1, "Synth", ChannelType::Audio);
        source_ch.set_input_index(0);

        float src_l[64], src_r[64];
        for (uint32_t i = 0; i < 64; ++i) {
            src_l[i] = 0.8f;
            src_r[i] = 0.3f;
        }

        AudioBuffer src_buf;
        src_buf.frames   = 64;
        src_buf.channels = 2;
        src_buf.capacity = 64;
        src_buf.data[0]  = src_l;
        src_buf.data[1]  = src_r;

        AudioBuffer* inputs[] = { &src_buf };

        sc.process(inputs, 1, 64,
            [&source_ch](ChannelID id) -> Channel* {
                return (id == 1) ? &source_ch : nullptr;
            });

        const auto* side_buf = sc.get_sidechain_buffer(2);
        REQUIRE(side_buf != nullptr);
        REQUIRE(side_buf->channels >= 2);
        for (uint32_t i = 0; i < 64; ++i) {
            REQUIRE(side_buf->data[0][i] == Catch::Approx(0.8f).margin(0.0001f));
            REQUIRE(side_buf->data[1][i] == Catch::Approx(0.3f).margin(0.0001f));
        }
    }

    SECTION("process with unknown source channel zeroes buffer") {
        sc.set_sidechain(1, 2); // source=1, but no channel with ID 1 exists
        sc.prepare(2, 64);

        float src_data[64];
        for (uint32_t i = 0; i < 64; ++i) src_data[i] = 0.5f;

        AudioBuffer src_buf;
        src_buf.frames   = 64;
        src_buf.channels = 1;
        src_buf.capacity = 64;
        src_buf.data[0]  = src_data;

        AudioBuffer* inputs[] = { &src_buf };

        // Lambda returns nullptr for all IDs — source not found
        sc.process(inputs, 1, 64, [](ChannelID) -> Channel* { return nullptr; });

        const auto* side_buf = sc.get_sidechain_buffer(2);
        REQUIRE(side_buf != nullptr);
        for (uint32_t i = 0; i < 64; ++i) {
            REQUIRE(side_buf->data[0][i] == Catch::Approx(0.0f).margin(0.0001f));
        }
    }

    SECTION("multiple sidechain connections process independently") {
        // source=1→target=2, source=3→target=4
        sc.set_sidechain(1, 2);
        sc.set_sidechain(3, 4);
        sc.prepare(2, 64);

        Channel src_a(1, "Kick", ChannelType::Audio);
        Channel src_b(3, "Snare", ChannelType::Audio);
        src_a.set_input_index(0);
        src_b.set_input_index(1);

        float kick[64], snare[64];
        for (uint32_t i = 0; i < 64; ++i) {
            kick[i]  = 1.0f;
            snare[i] = 0.5f;
        }

        AudioBuffer buf_a, buf_b;
        buf_a.frames = buf_b.frames = 64;
        buf_a.channels = buf_b.channels = 1;
        buf_a.capacity = buf_b.capacity = 64;
        buf_a.data[0] = kick;
        buf_b.data[0] = snare;

        AudioBuffer* inputs[] = { &buf_a, &buf_b };

        sc.process(inputs, 2, 64,
            [&src_a, &src_b](ChannelID id) -> Channel* {
                if (id == 1) return &src_a;
                if (id == 3) return &src_b;
                return nullptr;
            });

        const auto* sc1 = sc.get_sidechain_buffer(2);
        const auto* sc2 = sc.get_sidechain_buffer(4);
        REQUIRE(sc1 != nullptr);
        REQUIRE(sc2 != nullptr);

        for (uint32_t i = 0; i < 64; ++i) {
            REQUIRE(sc1->data[0][i] == Catch::Approx(1.0f).margin(0.0001f));
            REQUIRE(sc2->data[0][i] == Catch::Approx(0.5f).margin(0.0001f));
        }
    }
}
