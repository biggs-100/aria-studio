#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "model/audio_clip.h"

#include <cstdint>
#include <cmath>

using namespace aria;

// ─── WaveformCache LOD ──────────────────────────────────────────

TEST_CASE("AudioClip — WaveformCache LOD", "[model][audio_clip][waveform]") {
    SECTION("get_waveform returns correct number of peak values") {
        // Given an AudioClip referencing a 44100-sample file
        AudioClip clip;
        clip.set_total_samples(44100);

        // When get_waveform(441) is called
        const auto& wf = clip.get_waveform(441);

        // Then the cache returns 100 peak values (1 per 441 samples)
        REQUIRE(wf.peaks.size() == 100);
    }

    SECTION("waveform data has matching valleys array") {
        AudioClip clip;
        clip.set_total_samples(44100);

        const auto& wf = clip.get_waveform(441);
        REQUIRE(wf.peaks.size() == wf.valleys.size());
    }

    SECTION("multiple LOD levels are cached independently") {
        AudioClip clip;
        clip.set_total_samples(44100);

        const auto& low_res  = clip.get_waveform(441);    // 100 points
        const auto& high_res = clip.get_waveform(44);      // 1000 points (44100/44 ≈ 1002)

        // Lower resolution should have fewer points
        REQUIRE(low_res.peaks.size() < high_res.peaks.size());
    }

    SECTION("initial state has empty waveform") {
        AudioClip clip;
        REQUIRE(clip.waveform_cache().lods().empty());
    }

    SECTION("waveform data is bounded to [0, 1] range") {
        AudioClip clip;
        clip.set_total_samples(44100);

        const auto& wf = clip.get_waveform(441);
        for (size_t i = 0; i < wf.peaks.size(); ++i) {
            REQUIRE(wf.peaks[i] >= 0.0f);
            REQUIRE(wf.peaks[i] <= 1.0f);
            REQUIRE(wf.valleys[i] >= 0.0f);
            REQUIRE(wf.valleys[i] <= 1.0f);
        }
    }
}

// ─── Sample Offset ─────────────────────────────────────────────

TEST_CASE("AudioClip — sample offset", "[model][audio_clip][sample_offset]") {
    SECTION("sample_offset trims playback start") {
        AudioClip clip;
        clip.set_total_samples(88200);
        clip.set_sample_offset(44100);

        // Only the last 44100 samples of the source file are heard
        REQUIRE(clip.sample_offset() == 44100);
        REQUIRE(clip.playback_samples() == 44100);
    }

    SECTION("zero sample_offset uses full file") {
        AudioClip clip;
        clip.set_total_samples(88200);
        clip.set_sample_offset(0);

        REQUIRE(clip.playback_samples() == 88200);
    }

    SECTION("offset beyond total_samples clamps to total") {
        AudioClip clip;
        clip.set_total_samples(44100);
        clip.set_sample_offset(88200);

        // Clamped: no samples available
        REQUIRE(clip.playback_samples() == 0);
    }
}

// ─── Gain Envelope ─────────────────────────────────────────────

TEST_CASE("AudioClip — gain envelope", "[model][audio_clip][gain_envelope]") {
    SECTION("empty envelope applies unity gain") {
        AudioClip clip;
        REQUIRE(clip.gain_at_ppqn(0) == Catch::Approx(1.0));
        REQUIRE(clip.gain_at_ppqn(480) == Catch::Approx(1.0));
        REQUIRE(clip.gain_at_ppqn(960) == Catch::Approx(1.0));
    }

    SECTION("gain envelope ramps between points") {
        AudioClip clip;
        clip.add_gain_point({0, 0.0});    // PPQN 0 → gain 0.0
        clip.add_gain_point({960, 1.0});  // PPQN 960 → gain 1.0

        // At midpoint (PPQN 480), gain is 0.5
        REQUIRE(clip.gain_at_ppqn(480) == Catch::Approx(0.5));
    }

    SECTION("gain_at_ppqn clamps before first point") {
        AudioClip clip;
        clip.add_gain_point({480, 0.5});
        clip.add_gain_point({960, 1.0});

        // Before first point: returns first point value
        REQUIRE(clip.gain_at_ppqn(0) == Catch::Approx(0.5));
    }

    SECTION("gain_at_ppqn clamps after last point") {
        AudioClip clip;
        clip.add_gain_point({0, 0.0});
        clip.add_gain_point({480, 0.5});

        // After last point: returns last point value
        REQUIRE(clip.gain_at_ppqn(960) == Catch::Approx(0.5));
    }
}

// ─── AudioClip inherits from Clip ──────────────────────────────

TEST_CASE("AudioClip — inherits Clip base", "[model][audio_clip][clip_base]") {
    SECTION("AudioClip is a Clip") {
        AudioClip clip;
        Clip* base = &clip;
        REQUIRE(base != nullptr);
    }

    SECTION("AudioClip has ClipID") {
        AudioClip clip;
        REQUIRE(clip.id().value > 0);
    }

    SECTION("inherits position and length") {
        AudioClip clip;
        clip.set_start(0);
        clip.set_length(960);
        REQUIRE(clip.start() == 0);
        REQUIRE(clip.length() == 960);
        REQUIRE(clip.end() == 960);
    }

    SECTION("inherits fade evaluate") {
        REQUIRE(Clip::evaluate_fade(FadeShape::LinearIn, 0.5)
                == Catch::Approx(0.5));
    }
}
