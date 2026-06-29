#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mixer/mixer.h"
#include "audio/audio_buffer.h"
#include "test_common/audio_harness.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <memory>

using namespace aria;

// ─── Mixer Initialisation ───────────────────────────────────────

TEST_CASE("Mixer init and shutdown", "[mixer][engine]") {
    Mixer mixer;

    SECTION("init succeeds with sample rate") {
        REQUIRE(mixer.init(48000));
        REQUIRE(mixer.master_channel() != nullptr);
    }

    SECTION("init creates master channel") {
        mixer.init(48000);
        auto* master = mixer.master_channel();
        REQUIRE(master != nullptr);
        REQUIRE(master->type() == ChannelType::Master);
        REQUIRE(master->name() == "Master");
    }

    SECTION("shutdown clears all channels") {
        mixer.init(48000);
        mixer.create_channel("Test", ChannelType::Audio);
        mixer.shutdown();
        REQUIRE(mixer.all_channels().empty());
    }

    SECTION("second init after shutdown works") {
        mixer.init(48000);
        mixer.shutdown();
        REQUIRE(mixer.init(48000));
        REQUIRE(mixer.master_channel() != nullptr);
    }
}

// ─── Channel Creation / Destruction ─────────────────────────────

TEST_CASE("Mixer channel management", "[mixer][engine][channel]") {
    Mixer mixer;
    mixer.init(48000);

    SECTION("create_channel assigns unique IDs") {
        ChannelID id1 = mixer.create_channel("Ch1", ChannelType::Audio);
        ChannelID id2 = mixer.create_channel("Ch2", ChannelType::Audio);
        REQUIRE(id1 != id2);
        REQUIRE(id1 != kInvalidChannelID);
        REQUIRE(id2 != kInvalidChannelID);
    }

    SECTION("get_channel returns created channel") {
        ChannelID id = mixer.create_channel("Kick", ChannelType::Audio);
        auto* ch = mixer.get_channel(id);
        REQUIRE(ch != nullptr);
        REQUIRE(ch->name() == "Kick");
        REQUIRE(ch->type() == ChannelType::Audio);
    }

    SECTION("get_channel returns nullptr for unknown ID") {
        REQUIRE(mixer.get_channel(9999) == nullptr);
    }

    SECTION("destroy_channel removes channel") {
        ChannelID id = mixer.create_channel("Kick", ChannelType::Audio);
        REQUIRE(mixer.get_channel(id) != nullptr);
        mixer.destroy_channel(id);
        REQUIRE(mixer.get_channel(id) == nullptr);
    }

    SECTION("cannot destroy master channel") {
        auto* master = mixer.master_channel();
        ChannelID master_id = master->id();
        mixer.destroy_channel(master_id);
        REQUIRE(mixer.master_channel() != nullptr);
    }

    SECTION("all_channels returns all IDs") {
        ChannelID a = mixer.create_channel("A", ChannelType::Audio);
        ChannelID b = mixer.create_channel("B", ChannelType::Audio);
        auto ids = mixer.all_channels();
        REQUIRE(ids.size() == 3);
        // Verify the created IDs are in the list
        REQUIRE(std::find(ids.begin(), ids.end(), a) != ids.end());
        REQUIRE(std::find(ids.begin(), ids.end(), b) != ids.end());
    }

    SECTION("create_channel returns invalid at max capacity") {
        MixerConfig config;
        config.max_channels = 3; // Master + 2 extra
        mixer.set_config(config);
        REQUIRE(mixer.create_channel("A", ChannelType::Audio) != kInvalidChannelID);
        REQUIRE(mixer.create_channel("B", ChannelType::Audio) != kInvalidChannelID);
        REQUIRE(mixer.create_channel("C", ChannelType::Audio) == kInvalidChannelID);
    }

    SECTION("various channel types can be created") {
        REQUIRE(mixer.create_channel("Audio",  ChannelType::Audio)   != kInvalidChannelID);
        REQUIRE(mixer.create_channel("MIDI",   ChannelType::MIDI)    != kInvalidChannelID);
        REQUIRE(mixer.create_channel("Group",  ChannelType::Group)   != kInvalidChannelID);
        REQUIRE(mixer.create_channel("Return", ChannelType::Return)  != kInvalidChannelID);
        REQUIRE(mixer.create_channel("VCA",    ChannelType::VCA)     != kInvalidChannelID);
    }
}

// ─── Bus Management ─────────────────────────────────────────────

TEST_CASE("Mixer bus management", "[mixer][engine][bus]") {
    Mixer mixer;
    mixer.init(48000);

    SECTION("create_bus assigns unique IDs") {
        BusID b1 = mixer.create_bus("Drums");
        BusID b2 = mixer.create_bus("Vocals");
        REQUIRE(b1 != b2);
        REQUIRE(b1 != kInvalidBusID);
        REQUIRE(b2 != kInvalidBusID);
    }

    SECTION("bus_name returns assigned name") {
        BusID id = mixer.create_bus("Drums");
        REQUIRE(mixer.bus_name(id) == "Drums");
    }

    SECTION("bus_name returns empty for unknown") {
        REQUIRE(mixer.bus_name(9999).empty());
    }

    SECTION("assign_channel_to_bus and channel_bus") {
        ChannelID ch = mixer.create_channel("Kick", ChannelType::Audio);
        BusID bus = mixer.create_bus("Drums");
        mixer.assign_channel_to_bus(ch, bus);
        REQUIRE(mixer.channel_bus(ch) == bus);
    }

    SECTION("assigning to new bus replaces old assignment") {
        ChannelID ch = mixer.create_channel("Kick", ChannelType::Audio);
        BusID b1 = mixer.create_bus("Drums");
        BusID b2 = mixer.create_bus("Percussion");
        mixer.assign_channel_to_bus(ch, b1);
        mixer.assign_channel_to_bus(ch, b2);
        REQUIRE(mixer.channel_bus(ch) == b2);
    }

    SECTION("remove_channel_from_bus clears assignment") {
        ChannelID ch = mixer.create_channel("Kick", ChannelType::Audio);
        BusID bus = mixer.create_bus("Drums");
        mixer.assign_channel_to_bus(ch, bus);
        mixer.remove_channel_from_bus(ch);
        REQUIRE(mixer.channel_bus(ch) == kInvalidBusID);
    }

    SECTION("channel_bus returns invalid for unassigned") {
        ChannelID ch = mixer.create_channel("Kick", ChannelType::Audio);
        REQUIRE(mixer.channel_bus(ch) == kInvalidBusID);
    }

    SECTION("destroy_bus removes bus and clears assignments") {
        ChannelID ch = mixer.create_channel("Kick", ChannelType::Audio);
        BusID bus = mixer.create_bus("Drums");
        mixer.assign_channel_to_bus(ch, bus);
        mixer.destroy_bus(bus);
        REQUIRE(mixer.channel_bus(ch) == kInvalidBusID);
        REQUIRE(mixer.all_buses().empty());
    }

    SECTION("all_buses returns all bus IDs") {
        BusID b1 = mixer.create_bus("Drums");
        BusID b2 = mixer.create_bus("Vocals");
        auto ids = mixer.all_buses();
        REQUIRE(ids.size() == 2);
        REQUIRE(std::find(ids.begin(), ids.end(), b1) != ids.end());
        REQUIRE(std::find(ids.begin(), ids.end(), b2) != ids.end());
    }
}

// ─── Pan Law ────────────────────────────────────────────────────

TEST_CASE("Mixer pan law setting", "[mixer][engine][pan]") {
    Mixer mixer;
    mixer.init(48000);

    SECTION("default pan law is EqualPower_3dB") {
        REQUIRE(mixer.pan_law() == PanLaw::EqualPower_3dB);
    }

    SECTION("set_pan_law changes the law") {
        mixer.set_pan_law(PanLaw::Linear_0dB);
        REQUIRE(mixer.pan_law() == PanLaw::Linear_0dB);
    }
}

// ─── Processing: Basic Summing ──────────────────────────────────

TEST_CASE("Mixer basic summing", "[mixer][engine][process]") {
    Mixer mixer;
    mixer.init(48000);

    constexpr uint32_t kFrames = 64;
    constexpr uint32_t kChannels = 2;

    // Create a test channel
    ChannelID ch_id = mixer.create_channel("Test", ChannelType::Audio);
    auto* ch = mixer.get_channel(ch_id);
    REQUIRE(ch != nullptr);
    ch->set_input_index(0);
    ch->set_volume(0.0); // unity gain

    // Allocate input and output buffers
    float in0[kFrames];
    float in1[kFrames];
    float out0[kFrames];
    float out1[kFrames];

    for (uint32_t i = 0; i < kFrames; ++i) {
        in0[i] = 0.5f;
        in1[i] = 0.3f;
        out0[i] = 0.0f;
        out1[i] = 0.0f;
    }

    AudioBuffer input_buf;
    input_buf.frames   = kFrames;
    input_buf.channels = kChannels;
    input_buf.capacity = kFrames;
    input_buf.data[0]  = in0;
    input_buf.data[1]  = in1;

    AudioBuffer output_buf;
    output_buf.frames   = kFrames;
    output_buf.channels = kChannels;
    output_buf.capacity = kFrames;
    output_buf.data[0]  = out0;
    output_buf.data[1]  = out1;

    // Equal-power pan law attenuates by cos(π/4) = sin(π/4) ≈ 0.7071 at center
    static constexpr float kCenterPanGain = 0.70710678f;

    AudioBuffer* inputs[] = { &input_buf };

    SECTION("process copies input to master output at unity gain") {
        mixer.process(inputs, 1, &output_buf, kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out0[i] == Catch::Approx(0.5f * kCenterPanGain));
            REQUIRE(out1[i] == Catch::Approx(0.3f * kCenterPanGain));
        }
    }

    SECTION("process applies volume changes") {
        ch->set_volume(-6.0); // ~0.5 linear gain
        mixer.process(inputs, 1, &output_buf, kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            float expected = static_cast<float>(0.5 * Channel::db_to_linear(-6.0) * kCenterPanGain);
            REQUIRE(out0[i] == Catch::Approx(expected).margin(0.01f));
            float expected_r = static_cast<float>(0.3 * Channel::db_to_linear(-6.0) * kCenterPanGain);
            REQUIRE(out1[i] == Catch::Approx(expected_r).margin(0.01f));
        }
    }

    SECTION("muted channel produces silence") {
        ch->set_mute(true);
        mixer.process(inputs, 1, &output_buf, kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out0[i] == Catch::Approx(0.0f));
            REQUIRE(out1[i] == Catch::Approx(0.0f));
        }
    }

    SECTION("soloed-only: muted non-soloed channels silenced") {
        // Create a second channel
        ChannelID ch2_id = mixer.create_channel("Soloed", ChannelType::Audio);
        auto* ch2 = mixer.get_channel(ch2_id);
        ch2->set_input_index(1);
        ch2->set_volume(0.0);
        ch2->set_solo(true);

        float in2[kFrames];
        for (uint32_t i = 0; i < kFrames; ++i) in2[i] = 1.0f;

        AudioBuffer input2;
        input2.frames   = kFrames;
        input2.channels = 1;
        input2.capacity = kFrames;
        input2.data[0]  = in2;

        AudioBuffer* multi_inputs[] = { &input_buf, &input2 };
        mixer.process(multi_inputs, 2, &output_buf, kFrames);

        // First channel (not soloed) should be silent
        // Second channel (soloed) should pass through with center-pan attenuation
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out0[i] == Catch::Approx(1.0f * kCenterPanGain));
        }
    }

    SECTION("phase invert flips signal polarity") {
        ch->set_phase_invert(true);
        mixer.process(inputs, 1, &output_buf, kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out0[i] == Catch::Approx(-0.5f * kCenterPanGain));
            REQUIRE(out1[i] == Catch::Approx(-0.3f * kCenterPanGain));
        }
    }
}

// ─── Processing: Bus Summing ───────────────────────────────────

TEST_CASE("Mixer bus summing", "[mixer][engine][bus][process]") {
    Mixer mixer;
    mixer.init(48000);

    constexpr uint32_t kFrames = 64;

    // Create two audio channels assigned to a bus
    ChannelID ch1 = mixer.create_channel("Kick", ChannelType::Audio);
    ChannelID ch2 = mixer.create_channel("Snare", ChannelType::Audio);
    BusID bus = mixer.create_bus("Drums");

    mixer.assign_channel_to_bus(ch1, bus);
    mixer.assign_channel_to_bus(ch2, bus);

    auto* c1 = mixer.get_channel(ch1);
    auto* c2 = mixer.get_channel(ch2);
    c1->set_input_index(0);
    c2->set_input_index(1);
    c1->set_volume(0.0);
    c2->set_volume(0.0);

    // Input buffers
    float in0[kFrames], in1[kFrames], out0[kFrames], out1[kFrames];
    for (uint32_t i = 0; i < kFrames; ++i) {
        in0[i] = 0.5f;
        in1[i] = 0.3f;
        out0[i] = 0.0f;
        out1[i] = 0.0f;
    }

    AudioBuffer buf1, buf2, output;
    buf1.frames = buf2.frames = output.frames = kFrames;
    buf1.channels = buf2.channels = output.channels = 2;
    buf1.capacity = buf2.capacity = output.capacity = kFrames;
    buf1.data[0] = buf2.data[0] = in0;
    buf1.data[1] = buf2.data[1] = in1;
    output.data[0] = out0;
    output.data[1] = out1;

    // Equal-power pan law attenuates by cos(π/4) = sin(π/4) ≈ 0.7071 at center
    static constexpr float kCenterPanGain = 0.70710678f;

    AudioBuffer* inputs[] = { &buf1, &buf2 };

    SECTION("bus channels sum into bus then master") {
        mixer.process(inputs, 2, &output, kFrames);
        // Both channels have 0.5/0.3, each center-panned, summed through bus to master
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out0[i] == Catch::Approx(2.0f * 0.5f * kCenterPanGain));
            REQUIRE(out1[i] == Catch::Approx(2.0f * 0.3f * kCenterPanGain));
        }
    }
}

// ─── Processing: Master Channel ─────────────────────────────────

TEST_CASE("Mixer master channel volume", "[mixer][engine][master]") {
    Mixer mixer;
    mixer.init(48000);

    constexpr uint32_t kFrames = 64;

    ChannelID ch = mixer.create_channel("Test", ChannelType::Audio);
    mixer.get_channel(ch)->set_input_index(0);
    mixer.get_channel(ch)->set_volume(0.0);

    float in0[kFrames], out0[kFrames], out1[kFrames];
    for (uint32_t i = 0; i < kFrames; ++i) {
        in0[i] = 0.5f;
        out0[i] = 0.0f;
        out1[i] = 0.0f;
    }

    AudioBuffer input, output;
    input.frames = output.frames = kFrames;
    input.channels = 1;
    input.capacity = output.capacity = kFrames;
    input.data[0] = in0;
    output.data[0] = out0;
    output.data[1] = out1;
    output.channels = 2;

    // Equal-power pan law attenuates by cos(π/4) = sin(π/4) ≈ 0.7071 at center
    static constexpr float kCenterPanGain = 0.70710678f;

    AudioBuffer* inputs[] = { &input };

    SECTION("master volume affects all output") {
        auto* master = mixer.master_channel();
        master->set_volume(-6.0); // ~0.5 gain
        mixer.process(inputs, 1, &output, kFrames);
        float expected = static_cast<float>(0.5 * Channel::db_to_linear(-6.0) * kCenterPanGain);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out0[i] == Catch::Approx(expected).margin(0.01f));
        }
    }
}

// ─── Processing: Send Routing ──────────────────────────────────

TEST_CASE("Mixer send routing accumulation", "[mixer][engine][sends][process]") {
    Mixer mixer;
    mixer.init(48000);

    constexpr uint32_t kFrames = 64;

    // Create a single audio channel with a send to master
    ChannelID ch = mixer.create_channel("Guitar", ChannelType::Audio);
    auto* ch_ptr = mixer.get_channel(ch);
    ch_ptr->set_input_index(0);
    ch_ptr->set_volume(0.0); // unity gain

    ChannelID master_id = mixer.master_channel()->id();

    float in[kFrames], out0[kFrames], out1[kFrames];
    for (uint32_t i = 0; i < kFrames; ++i) {
        in[i]   = 1.0f;
        out0[i] = out1[i] = 0.0f;
    }

    AudioBuffer input, output;
    input.frames = output.frames = kFrames;
    input.channels = 1;
    input.capacity = output.capacity = kFrames;
    input.data[0]  = in;
    output.data[0] = out0;
    output.data[1] = out1;
    output.channels = 2;

    AudioBuffer* inputs[] = { &input };

    static constexpr float kCenterPanGain = 0.70710678f;

    SECTION("send audio accumulates into master along with direct signal") {
        ch_ptr->add_send(master_id, -6.0, true);
        mixer.process(inputs, 1, &output, kFrames);

        // Direct: 1.0 * kCenterPanGain (unity volume)
        // Send (pre-fader, -6 dB): 1.0 * 0.5 * kCenterPanGain
        float expected = kCenterPanGain * (1.0f + 0.5f);

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out0[i] == Catch::Approx(expected).margin(0.01f));
        }
    }

    SECTION("no send produces only direct signal") {
        mixer.process(inputs, 1, &output, kFrames);

        float expected = kCenterPanGain;
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out0[i] == Catch::Approx(expected).margin(0.01f));
        }
    }

    SECTION("pre-fader send is unaffected by channel volume change") {
        ch_ptr->add_send(master_id, -6.0, true);
        ch_ptr->set_volume(-12.0); // reduce channel volume

        mixer.process(inputs, 1, &output, kFrames);

        // Direct path attenuated: 1.0 * kCenterPanGain * db_to_linear(-12)
        // Send pre-fader still at full: 1.0 * 0.5 * kCenterPanGain
        float direct   = kCenterPanGain * static_cast<float>(Channel::db_to_linear(-12.0));
        float send     = kCenterPanGain * 0.5f;
        float expected = direct + send;

        REQUIRE(out0[0] == Catch::Approx(expected).margin(0.01f));
    }

    SECTION("post-fader send tracks channel volume") {
        ch_ptr->add_send(master_id, -6.0, false); // post-fader
        ch_ptr->set_volume(0.0); // unity

        mixer.process(inputs, 1, &output, kFrames);
        float expected_unity = kCenterPanGain * (1.0f + 0.5f);
        REQUIRE(out0[0] == Catch::Approx(expected_unity).margin(0.01f));

        // Now reduce channel volume — post-fader send should also reduce
        ch_ptr->set_volume(-12.0);
        mixer.process(inputs, 1, &output, kFrames);

        float vol_gain = static_cast<float>(Channel::db_to_linear(-12.0));
        float direct   = kCenterPanGain * vol_gain;
        float send     = kCenterPanGain * vol_gain * 0.5f; // post-fader: volume affects send
        float expected = direct + send;

        REQUIRE(out0[0] == Catch::Approx(expected).margin(0.01f));
    }
}

// ─── Processing: Edge Cases ────────────────────────────────────

TEST_CASE("Mixer process edge cases", "[mixer][engine][process]") {
    Mixer mixer;
    mixer.init(48000);

    SECTION("process with no channels produces silence") {
        float out0[32] = {}, out1[32] = {};
        AudioBuffer output;
        output.frames = 32;
        output.channels = 2;
        output.capacity = 32;
        output.data[0] = out0;
        output.data[1] = out1;

        mixer.process(nullptr, 0, &output, 32);
        for (uint32_t i = 0; i < 32; ++i) {
            REQUIRE(out0[i] == Catch::Approx(0.0f));
            REQUIRE(out1[i] == Catch::Approx(0.0f));
        }
    }

    SECTION("process with zero frames does nothing") {
        float out0[32] = {42.0f};
        AudioBuffer output;
        output.frames = 32;
        output.channels = 1;
        output.capacity = 32;
        output.data[0] = out0;

        mixer.process(nullptr, 0, &output, 0);
        // Should not crash, and out0 shouldn't be zeroed
        REQUIRE(out0[0] == Catch::Approx(42.0f));
    }

    SECTION("config can be set and read") {
        MixerConfig cfg;
        cfg.max_channels = 64;
        cfg.max_buses = 16;
        mixer.set_config(cfg);
        REQUIRE(mixer.config().max_channels == 64);
        REQUIRE(mixer.config().max_buses == 16);
    }
}
