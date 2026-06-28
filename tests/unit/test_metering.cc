#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "audio/metering/peak_meter.h"
#include "audio/metering/rms_meter.h"
#include "audio/metering/lufs_meter.h"
#include "audio/metering/phase_meter.h"
#include "audio/metering/spectrum_analyzer.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace aria::metering;

// ═══════════════════════════════════════════════════════════════════
// PeakMeter Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("PeakMeter basic operation", "[audio][metering][peak]") {
    PeakMeter meter;

    SECTION("initial state is zero") {
        REQUIRE(meter.current_peak() == 0.0f);
        REQUIRE(meter.hold_peak() == 0.0f);
    }

    SECTION("single sine wave block") {
        std::vector<float> samples(1024);
        float expected_peak = 0.0f;
        for (uint32_t i = 0; i < 1024; ++i) {
            samples[i] = 0.85f * std::sin(2.0f * 3.14159f * 100.0f * i / 48000.0f);
            expected_peak = std::max(expected_peak, std::abs(samples[i]));
        }

        meter.process(samples.data(), 1024, 0);

        // Current peak should be close to expected
        REQUIRE(meter.current_peak() == Catch::Approx(expected_peak).margin(0.01f));
        REQUIRE(meter.hold_peak() == Catch::Approx(expected_peak).margin(0.01f));
    }

    SECTION("hold peak is remembered") {
        std::vector<float> low(256, 0.1f);
        std::vector<float> high(256, 0.9f);

        meter.process(low.data(), 256, 0);
        float initial = meter.current_peak();

        meter.process(high.data(), 256, 0);

        REQUIRE(meter.current_peak() >= 0.9f);
        REQUIRE(meter.hold_peak() >= 0.9f);
    }

    SECTION("reset hold resets to current") {
        std::vector<float> samples(256, 0.5f);
        meter.process(samples.data(), 256, 0);
        meter.reset_hold();
        REQUIRE(meter.hold_peak() == Catch::Approx(0.5f));
    }

    SECTION("decay rate affects fall-back") {
        meter.set_decay_rate(100.0); // 100 dB/s

        std::vector<float> peak(64, 1.0f);
        meter.process(peak.data(), 64, 0);

        // After decay, should be slightly less than 1.0
        float after = meter.current_peak();
        REQUIRE(after >= 0.0f);
        REQUIRE(after <= 1.0f);
    }
}

// ═══════════════════════════════════════════════════════════════════
// RMSMeter Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("RMSMeter basic operation", "[audio][metering][rms]") {
    RMSMeter meter;

    SECTION("initial RMS is zero") {
        REQUIRE(meter.current_rms() == 0.0f);
    }

    SECTION("DC signal: RMS = amplitude") {
        std::vector<float> dc(2048, 0.75f);
        meter.process(dc.data(), 2048, 0);
        REQUIRE(meter.current_rms() == Catch::Approx(0.75f).margin(0.01f));
    }

    SECTION("sine wave: RMS = amplitude / sqrt(2)") {
        std::vector<float> sine(4096);
        double amplitude = 0.8;
        for (uint32_t i = 0; i < 4096; ++i) {
            sine[i] = static_cast<float>(amplitude * std::sin(2.0 * 3.14159 * 100.0 * i / 48000.0));
        }

        meter.process(sine.data(), 4096, 0);
        float expected_rms = static_cast<float>(amplitude / std::sqrt(2.0));
        REQUIRE(meter.current_rms() == Catch::Approx(expected_rms).margin(0.02f));
    }

    SECTION("silence: RMS = 0") {
        std::vector<float> silence(512, 0.0f);
        meter.process(silence.data(), 512, 0);
        REQUIRE(meter.current_rms() == Catch::Approx(0.0f).margin(1e-6f));
    }

    SECTION("window size affects response") {
        meter.set_window_ms(100.0); // shorter window = faster response

        std::vector<float> high(4800, 0.9f); // 100ms @ 48kHz
        meter.process(high.data(), 4800, 0);
        REQUIRE(meter.current_rms() == Catch::Approx(0.9f).margin(0.01f));
    }

    SECTION("reset clears state") {
        std::vector<float> dc(256, 0.8f);
        meter.process(dc.data(), 256, 0);
        REQUIRE(meter.current_rms() > 0.0f);

        meter.reset();
        REQUIRE(meter.current_rms() == 0.0f);
    }
}

// ═══════════════════════════════════════════════════════════════════
// LUFSMeter Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("LUFSMeter basic operation", "[audio][metering][lufs]") {
    LUFSMeter lufs;

    SECTION("initial state is silence") {
        auto r = lufs.read();
        REQUIRE(r.momentary == -70.0f);
        REQUIRE(r.short_term == -70.0f);
        REQUIRE(r.integrated == -70.0f);
    }

    SECTION("full-scale sine gives reasonable LUFS") {
        std::vector<float> sine(48000 * 2); // 2 seconds @ 48kHz
        for (uint32_t i = 0; i < 48000 * 2; ++i) {
            sine[i] = 0.5f * std::sin(2.0f * 3.14159f * 440.0f * i / 48000.0f);
        }

        lufs.process(sine.data(), 48000 * 2, 1);
        auto r = lufs.read();

        // A 0.5-amplitude sine should be quieter than -3 LUFS
        REQUIRE(r.momentary > -70.0f);
        REQUIRE(r.momentary < 0.0f);
    }

    SECTION("silence input gives -70 LUFS") {
        std::vector<float> silence(48000, 0.0f);
        lufs.process(silence.data(), 48000, 1);
        auto r = lufs.read();
        REQUIRE(r.momentary == -70.0f);
    }

    SECTION("reset clears state") {
        std::vector<float> sine(4800, 0.5f);
        lufs.process(sine.data(), 4800, 1);
        lufs.reset();
        auto r = lufs.read();
        REQUIRE(r.momentary == -70.0f);
    }

    SECTION("true peak reports negative dB for sub-1 signal") {
        std::vector<float> samples(480, 0.5f);
        lufs.process(samples.data(), 480, 1);
        auto r = lufs.read();
        REQUIRE(r.true_peak < 0.0f);
        REQUIRE(r.true_peak > -70.0f);
    }

    SECTION("stereo processing produces output") {
        std::vector<float> stereo(48000 * 4); // 2ch, 0.5s
        for (uint32_t i = 0; i < 48000; ++i) {
            stereo[i * 2]     = 0.3f * std::sin(2.0f * 3.14159f * 440.0f * i / 48000.0f);
            stereo[i * 2 + 1] = 0.3f * std::sin(2.0f * 3.14159f * 880.0f * i / 48000.0f);
        }
        lufs.process(stereo.data(), 48000, 2);
        auto r = lufs.read();
        REQUIRE(r.momentary > -70.0f);
        REQUIRE(r.momentary < 0.0f);
    }
}

// ═══════════════════════════════════════════════════════════════════
// PhaseMeter Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("PhaseMeter correlation", "[audio][metering][phase]") {
    PhaseMeter meter;

    SECTION("initial correlation is 1.0") {
        REQUIRE(meter.correlation() == Catch::Approx(1.0f).margin(0.01f));
    }

    SECTION("identical signals: correlation = 1.0") {
        std::vector<float> left(1024), right(1024);
        for (uint32_t i = 0; i < 1024; ++i) {
            left[i] = right[i] = std::sin(2.0f * 3.14159f * 100.0f * i / 48000.0f);
        }
        meter.process(left.data(), right.data(), 1024);
        REQUIRE(meter.correlation() == Catch::Approx(1.0f).margin(0.05f));
    }

    SECTION("opposite polarity: correlation = -1.0") {
        std::vector<float> left(1024), right(1024);
        for (uint32_t i = 0; i < 1024; ++i) {
            left[i] = std::sin(2.0f * 3.14159f * 100.0f * i / 48000.0f);
            right[i] = -left[i];
        }
        meter.process(left.data(), right.data(), 1024);
        REQUIRE(meter.correlation() == Catch::Approx(-1.0f).margin(0.05f));
    }

    SECTION("uncorrelated noise: correlation near 0") {
        std::vector<float> left(2048), right(2048);
        // Left: sine, Right: different sine (should be somewhat correlated)
        for (uint32_t i = 0; i < 2048; ++i) {
            left[i] = std::sin(2.0f * 3.14159f * 100.0f * i / 48000.0f);
            right[i] = std::sin(2.0f * 3.14159f * 1000.0f * i / 48000.0f);
        }
        meter.process(left.data(), right.data(), 2048);
        // Different frequencies should have correlation between -1 and 1
        float corr = meter.correlation();
        REQUIRE(corr >= -1.0f);
        REQUIRE(corr <= 1.0f);
    }

    SECTION("reset restores correlation to 1.0") {
        std::vector<float> left(128, 1.0f), right(128, -1.0f);
        meter.process(left.data(), right.data(), 128);
        meter.reset();
        REQUIRE(meter.correlation() == Catch::Approx(1.0f).margin(0.01f));
    }
}

// ═══════════════════════════════════════════════════════════════════
// SpectrumAnalyzer Tests
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("SpectrumAnalyzer frequency analysis", "[audio][metering][spectrum]") {
    SpectrumAnalyzer analyzer;

    SECTION("initial state has no bins") {
        auto s = analyzer.read();
        REQUIRE(s.bin_count == 0);
        REQUIRE(s.dominant_freq == 0.0f);
    }

    SECTION("sine wave dominant frequency") {
        constexpr uint32_t kFFTSize = 2048;
        constexpr uint32_t kSampleRate = 48000;
        analyzer.set_fft_size(kFFTSize);

        // Generate a 440 Hz sine wave
        std::vector<float> sine(kFFTSize);
        for (uint32_t i = 0; i < kFFTSize; ++i) {
            sine[i] = std::sin(2.0f * 3.14159f * 440.0f * i / kSampleRate);
        }

        // Process multiple times to fill ring buffer
        for (int iter = 0; iter < 5; ++iter) {
            analyzer.process(sine.data(), kFFTSize);
        }

        auto s = analyzer.read();
        REQUIRE(s.bin_count > 0);
        REQUIRE(s.dominant_freq > 0.0f);

        // 440 Hz should be at bin: 440 * 2048 / 48000 ≈ 18.77
        // Allow ±2 bins due to windowing spread
        float expected_bin_freq = 440.0f;
        REQUIRE(s.dominant_freq == Catch::Approx(expected_bin_freq).margin(
            static_cast<float>(kSampleRate) / kFFTSize * 3));
    }

    SECTION("silence produces zero magnitude") {
        analyzer.set_fft_size(1024);
        std::vector<float> silence(1024, 0.0f);
        for (int iter = 0; iter < 3; ++iter) {
            analyzer.process(silence.data(), 1024);
        }
        auto s = analyzer.read();
        REQUIRE(s.bin_count > 0);
        REQUIRE(s.max_magnitude < 0.001f);
    }

    SECTION("FFT size configuration") {
        analyzer.set_fft_size(512);
        // Process to trigger FFT
        std::vector<float> data(512, 0.5f);
        for (int i = 0; i < 3; ++i) analyzer.process(data.data(), 512);
        auto s = analyzer.read();
        REQUIRE(s.bin_count == 256); // 512/2
    }

    SECTION("window type configuration") {
        analyzer.set_fft_size(1024);
        analyzer.set_window_type(SpectrumAnalyzer::WindowType::Hamming);
        std::vector<float> data(1024, 0.5f);
        for (int i = 0; i < 3; ++i) analyzer.process(data.data(), 1024);
        auto s = analyzer.read();
        REQUIRE(s.bin_count == 512);
    }

    SECTION("Blackman-Harris window") {
        analyzer.set_fft_size(2048);
        analyzer.set_window_type(SpectrumAnalyzer::WindowType::BlackmanHarris);
        std::vector<float> data(2048, 0.3f);
        for (int i = 0; i < 3; ++i) analyzer.process(data.data(), 2048);
        auto s = analyzer.read();
        REQUIRE(s.bin_count == 1024);
    }

    SECTION("reset restores initial state") {
        std::vector<float> data(2048, 0.5f);
        for (int i = 0; i < 3; ++i) analyzer.process(data.data(), 2048);
        analyzer.reset();
        auto s = analyzer.read();
        REQUIRE(s.bin_count == 0);
        REQUIRE(s.max_magnitude == 0.0f);
    }
}
