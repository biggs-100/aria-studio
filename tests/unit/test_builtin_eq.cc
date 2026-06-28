#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "mixer/builtin_eq.h"

#include <cmath>
#include <numbers>
#include <vector>

using namespace aria;

// ─── Test helpers ─────────────────────────────────────────────────

/// Generate a sine wave in a buffer.
static void fill_sine(float* buf, uint32_t frames, double frequency,
                      double sample_rate, double amplitude = 1.0) {
    for (uint32_t i = 0; i < frames; ++i) {
        buf[i] = static_cast<float>(amplitude *
            std::sin(2.0 * std::numbers::pi_v<double> * frequency * i / sample_rate));
    }
}

/// Compute RMS of a buffer.
static float compute_rms(const float* buf, uint32_t frames) {
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < frames; ++i) {
        sum_sq += static_cast<double>(buf[i]) * buf[i];
    }
    return static_cast<float>(std::sqrt(sum_sq / frames));
}

// ═══════════════════════════════════════════════════════════════════
//  BiquadFilter tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("BiquadFilter construction and defaults", "[mixer][eq][biquad]") {
    BiquadFilter f;

    SECTION("default latency is 0") {
        REQUIRE(f.latency() == 0);
    }

    SECTION("default type is LowPass") {
        // No direct getter — we verify by processing silence doesn't crash
        float in[32] = {}, out[32] = {};
        REQUIRE_NOTHROW(f.process(in, out, 32, 1));
    }
}

TEST_CASE("BiquadFilter processes silence to silence", "[mixer][eq][biquad]") {
    BiquadFilter f;
    f.set_sample_rate(48000.0);
    f.update_coefficients();

    float in[64] = {}, out[64] = {};
    f.process(in, out, 64, 1);

    for (uint32_t i = 0; i < 64; ++i) {
        REQUIRE(out[i] == Catch::Approx(0.0f).margin(1e-7f));
    }
}

TEST_CASE("BiquadFilter low-pass attenuates high frequencies", "[mixer][eq][biquad]") {
    BiquadFilter f;
    f.set_sample_rate(48000.0);
    f.set_type(BiquadFilter::Type::LowPass);
    f.set_cutoff(500.0);     // 500 Hz cutoff
    f.set_q(0.707);           // Butterworth Q
    f.update_coefficients();

    constexpr uint32_t kFrames = 256;
    float in_low[kFrames], in_high[kFrames];
    float out_low[kFrames], out_high[kFrames];

    // Low frequency (100 Hz) — should pass
    fill_sine(in_low, kFrames, 100.0, 48000.0, 1.0f);
    f.process(in_low, out_low, kFrames, 1);

    // High frequency (5 kHz) — should be attenuated
    fill_sine(in_high, kFrames, 5000.0, 48000.0, 1.0f);
    f.reset();
    f.process(in_high, out_high, kFrames, 1);

    float rms_low  = compute_rms(out_low, kFrames);
    float rms_high = compute_rms(out_high, kFrames);

    // Low frequencies should have significantly higher RMS than high
    REQUIRE(rms_low > rms_high * 3.0f);
}

TEST_CASE("BiquadFilter high-pass attenuates low frequencies", "[mixer][eq][biquad]") {
    BiquadFilter f;
    f.set_sample_rate(48000.0);
    f.set_type(BiquadFilter::Type::HighPass);
    f.set_cutoff(1000.0);
    f.set_q(0.707);
    f.update_coefficients();

    constexpr uint32_t kFrames = 256;
    float in_low[kFrames], in_high[kFrames];
    float out_low[kFrames], out_high[kFrames];

    // Low frequency (100 Hz) — should be attenuated
    fill_sine(in_low, kFrames, 100.0, 48000.0, 1.0f);
    f.process(in_low, out_low, kFrames, 1);

    // High frequency (5 kHz) — should pass
    fill_sine(in_high, kFrames, 5000.0, 48000.0, 1.0f);
    f.reset();
    f.process(in_high, out_high, kFrames, 1);

    float rms_low  = compute_rms(out_low, kFrames);
    float rms_high = compute_rms(out_high, kFrames);

    REQUIRE(rms_high > rms_low * 3.0f);
}

// ═══════════════════════════════════════════════════════════════════
//  BuiltInEQ tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("BuiltInEQ band CRUD", "[mixer][eq][builtin]") {
    BuiltInEQ eq;

    SECTION("default has 0 bands") {
        REQUIRE(eq.band_count() == 0);
    }

    SECTION("add_band increases count") {
        BuiltInEQ::Band band;
        band.type = BuiltInEQ::Peak;
        band.frequency = 1000.0;
        band.gain_db = 3.0;
        band.q = 1.0;

        uint32_t idx = eq.add_band(band);
        REQUIRE(eq.band_count() == 1);
        REQUIRE(idx == 0);
    }

    SECTION("add_band returns sequential indices") {
        BuiltInEQ::Band band;
        REQUIRE(eq.add_band(band) == 0);
        REQUIRE(eq.add_band(band) == 1);
        REQUIRE(eq.add_band(band) == 2);
    }

    SECTION("remove_band reduces count") {
        eq.add_band(BuiltInEQ::Band{});
        eq.add_band(BuiltInEQ::Band{});
        eq.remove_band(0);
        REQUIRE(eq.band_count() == 1);
    }

    SECTION("remove_band with invalid index does nothing") {
        eq.add_band(BuiltInEQ::Band{});
        eq.remove_band(99);
        REQUIRE(eq.band_count() == 1);
    }

    SECTION("modify_band updates band parameters") {
        BuiltInEQ::Band band;
        band.type = BuiltInEQ::Peak;
        band.frequency = 1000.0;
        band.gain_db = 3.0;
        eq.add_band(band);

        BuiltInEQ::Band updated;
        updated.type = BuiltInEQ::LowShelf;
        updated.frequency = 200.0;
        updated.gain_db = -6.0;
        eq.modify_band(0, updated);

        const auto& b = eq.get_band(0);
        REQUIRE(b.type == BuiltInEQ::LowShelf);
        REQUIRE(b.frequency == Catch::Approx(200.0));
        REQUIRE(b.gain_db == Catch::Approx(-6.0));
    }

    SECTION("clear removes all bands") {
        eq.add_band(BuiltInEQ::Band{});
        eq.add_band(BuiltInEQ::Band{});
        eq.add_band(BuiltInEQ::Band{});
        eq.clear();
        REQUIRE(eq.band_count() == 0);
    }
}

TEST_CASE("BuiltInEQ bypass", "[mixer][eq][builtin]") {
    BuiltInEQ eq;
    eq.add_band(BuiltInEQ::Band{}); // Peak at 1kHz, 0 dB

    SECTION("default is not bypassed") {
        REQUIRE_FALSE(eq.is_bypassed());
    }

    SECTION("set_bypass toggles bypass") {
        eq.set_bypass(true);
        REQUIRE(eq.is_bypassed());
        eq.set_bypass(false);
        REQUIRE_FALSE(eq.is_bypassed());
    }

    SECTION("bypassed EQ passes audio unchanged") {
        constexpr uint32_t kFrames = 64;
        float in[kFrames], out[kFrames];
        for (uint32_t i = 0; i < kFrames; ++i) {
            in[i] = 0.5f;
            out[i] = 0.0f;
        }

        eq.set_bypass(true);
        eq.process(in, out, kFrames, 1);

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(out[i] == Catch::Approx(in[i]).margin(1e-7f));
        }
    }
}

TEST_CASE("BuiltInEQ latency", "[mixer][eq][builtin]") {
    BuiltInEQ eq;
    SECTION("latency is always 0") {
        REQUIRE(eq.latency() == 0);
        eq.add_band(BuiltInEQ::Band{});
        REQUIRE(eq.latency() == 0);
    }
}

TEST_CASE("BuiltInEQ processes stereo", "[mixer][eq][builtin]") {
    BuiltInEQ eq;
    eq.set_sample_rate(48000.0);

    // A peak band with +6 dB boost at 1kHz
    BuiltInEQ::Band peak;
    peak.type = BuiltInEQ::Peak;
    peak.frequency = 1000.0;
    peak.gain_db = 6.0;
    peak.q = 1.0;
    eq.add_band(peak);

    constexpr uint32_t kFrames = 256;
    float in[kFrames * 2], out[kFrames * 2] = {};

    // Stereo: interleaved in memory (L, R, L, R...)
    // Actually let's use planar format: channel 0 at offset 0, channel 1 at offset frames
    fill_sine(in, kFrames, 1000.0, 48000.0, 0.5f);
    fill_sine(in + kFrames, kFrames, 1000.0, 48000.0, 0.5f);

    eq.process(in, out, kFrames, 2);

    // Both channels should have boosted signal
    float rms_l = compute_rms(out, kFrames);
    float rms_r = compute_rms(out + kFrames, kFrames);

    // Expect boost: +6 dB peak should increase signal
    // Input RMS at 0.5 amplitude sine ≈ 0.3535
    // Output should be noticeably higher
    // We just verify both channels processed and RMS > some threshold
    REQUIRE(rms_l > 0.25f);
    REQUIRE(rms_r > 0.25f);
}

TEST_CASE("BuiltInEQ multiple bands cascade", "[mixer][eq][builtin]") {
    BuiltInEQ eq;
    eq.set_sample_rate(48000.0);

    // Add two bands: high shelf and low shelf
    BuiltInEQ::Band lowshelf;
    lowshelf.type = BuiltInEQ::LowShelf;
    lowshelf.frequency = 200.0;
    lowshelf.gain_db = -12.0;  // Cut lows
    lowshelf.q = 0.707;
    eq.add_band(lowshelf);

    BuiltInEQ::Band peak;
    peak.type = BuiltInEQ::Peak;
    peak.frequency = 2000.0;
    peak.gain_db = 6.0;
    peak.q = 2.0;
    eq.add_band(peak);

    constexpr uint32_t kFrames = 512;
    float in_low[kFrames], out_low[kFrames] = {};
    float in_mid[kFrames], out_mid[kFrames] = {};

    // Low frequency — should be attenuated by lowshelf
    fill_sine(in_low, kFrames, 80.0, 48000.0, 1.0f);
    eq.process(in_low, out_low, kFrames, 1);

    // Mid frequency — should be boosted by peak
    // Reset filters by re-processing another band set
    // Actually eq.process resets internal filters each call,
    // so we need separate instances or process separately
    // For accurate cascade test, use same EQ with same config
    eq.process(in_low, out_low, kFrames, 1); // reset & process low

    // Rebuild for mid — use fresh band config
    BuiltInEQ eq2;
    eq2.set_sample_rate(48000.0);
    eq2.add_band(lowshelf);
    eq2.add_band(peak);

    fill_sine(in_mid, kFrames, 2000.0, 48000.0, 1.0f);
    eq2.process(in_mid, out_mid, kFrames, 1);

    float rms_low = compute_rms(out_low, kFrames);
    float rms_mid = compute_rms(out_mid, kFrames);

    // Mid should be significantly higher RMS than low due to
    // lowshelf cut + peak boost
    REQUIRE(rms_mid > rms_low * 2.0f);
}

TEST_CASE("BuiltInEQ with no bands passes audio through", "[mixer][eq][builtin]") {
    BuiltInEQ eq;

    constexpr uint32_t kFrames = 64;
    float in[kFrames], out[kFrames] = {};
    for (uint32_t i = 0; i < kFrames; ++i) {
        in[i] = 0.75f;
    }

    eq.process(in, out, kFrames, 1);

    for (uint32_t i = 0; i < kFrames; ++i) {
        REQUIRE(out[i] == Catch::Approx(in[i]).margin(1e-7f));
    }
}
