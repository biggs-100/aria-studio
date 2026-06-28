# ARIA DAW — Testing Strategy

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27

---

## Table of Contents

1. [Overview](#1-overview)
2. [Test Pyramid](#2-test-pyramid)
3. [Unit Testing](#3-unit-testing)
4. [Integration Testing](#4-integration-testing)
5. [Audio Testing](#5-audio-testing)
6. [Plugin Testing](#6-plugin-testing)
7. [UI Testing](#7-ui-testing)
8. [Performance Testing](#8-performance-testing)
9. [Regression Testing](#9-regression-testing)
10. [CI/CD Integration](#10-cicd-integration)
11. [Test Infrastructure](#11-test-infrastructure)
12. [Coverage Targets](#12-coverage-targets)
13. [RFC Index](#13-rfc-index)

---

## 1. Overview

### 1.1. Purpose

ARIA DAW's testing strategy ensures correctness, performance, and stability across all modules. The approach is pragmatic — heavy testing on the audio/DSP core (where bugs are catastrophic), lighter testing on the UI layer (where iteration speed matters).

### 1.2. Testing Principles

```
1. Audio correctness is non-negotiable — sample-exact testing
2. Test at module boundaries, not implementation internals
3. Every bug fix requires a regression test
4. Performance tests run on every CI build
5. Plugin compatibility tested against reference implementations
6. UI tests focus on state logic, not pixel matching
```

---

## 2. Test Pyramid

### 2.1. Test Distribution

```
                    ╱╲
                   ╱  ╲        UI & Integration Tests
                  ╱    ╲       (~15% of tests)
                 ╱──────╲
                ╱        ╲     Module Integration Tests
               ╱          ╲    (~25%)
              ╱────────────╲
             ╱              ╲  Unit Tests (C++ & Rust)
            ╱                ╲ (~60%)
           ╱──────────────────╲
          
Test run times:
  Unit:          < 30 seconds
  Integration:   < 5 minutes
  UI:            < 15 minutes
  Full suite:    < 30 minutes
```

### 2.2. Test Types by Module

| Module | Unit | Integration | Performance | Format |
|---|---|---|---|---|
| Audio Engine | ✅ | ✅ | ✅ | C++ (Catch2) |
| MIDI Engine | ✅ | ✅ | ✅ | C++ (Catch2) |
| DSP Library | ✅ | ✅ | ✅ | C++ (Catch2) |
| Plugin Host | ✅ | ✅ | ✅ | C++ (Catch2) |
| Automation | ✅ | ✅ | ✅ | C++ (Catch2) |
| Project Format | ✅ | ✅ | — | C++ (Catch2) |
| Track Model | ✅ | ✅ | — | C++ (Catch2) |
| Mixer | ✅ | ✅ | ✅ | C++ (Catch2) |
| Database | ✅ | ✅ | ✅ | Rust (cargo test) |
| File Indexing | ✅ | ✅ | — | Rust (cargo test) |
| Lua Scripting | ✅ | ✅ | — | C++ + Lua |
| UI Components | ✅ | ✅ | — | TypeScript (Vitest) |
| UI Logic | ✅ | ⚠️ | — | TypeScript (Vitest) |

---

## 3. Unit Testing

### 3.1. C++ Tests (Catch2)

```cpp
// Example: DSP filter test
TEST_CASE("Biquad filter processes correctly", "[dsp]") {
    BiquadFilter filter;
    filter.set_sample_rate(48000);
    filter.set_type(BiquadFilter::Type::LowPass);
    filter.set_cutoff(1000);
    filter.set_resonance(0.707);
    
    SECTION("DC input passes through low-pass") {
        std::vector<float> input(128, 1.0f);
        std::vector<float> output(128);
        
        filter.process(input.data(), output.data(), 128);
        
        // DC should pass through (gain ≈ 1.0 at 0 Hz)
        REQUIRE(output[127] == Approx(1.0f).margin(0.01));
    }
    
    SECTION("High frequency is attenuated") {
        // Generate 10kHz sine wave
        std::vector<float> input(128);
        std::vector<float> output(128);
        for (int i = 0; i < 128; i++) {
            input[i] = sinf(2.0f * M_PI * 10000.0f * i / 48000.0f);
        }
        
        filter.process(input.data(), output.data(), 128);
        
        // After settling, output should be significantly lower
        float input_rms = calculate_rms(input);
        float output_rms = calculate_rms(output);
        REQUIRE(output_rms < input_rms * 0.5f);
    }
}
```

### 3.2. Rust Tests

```rust
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_sample_database_insert_and_query() {
        let db = SampleDB::open_in_memory().unwrap();
        
        db.insert_sample(SampleEntry {
            path: "/samples/kick.wav".into(),
            name: "Kick 01".into(),
            bpm: Some(120.0),
            key: Some("Cm".into()),
            category: "kick".into(),
            duration_ms: 2400,
        }).unwrap();
        
        let results = db.search("kick").unwrap();
        assert_eq!(results.len(), 1);
        assert_eq!(results[0].name, "Kick 01");
    }
    
    #[test]
    fn test_fts5_search_ranking() {
        let db = SampleDB::open_in_memory().unwrap();
        
        db.insert_sample(make_entry("dark_kick.wav", "dark kick with sub")).unwrap();
        db.insert_sample(make_entry("light_snare.wav", "bright snare")).unwrap();
        db.insert_sample(make_entry("dark_pad.wav", "dark ambient pad")).unwrap();
        
        let results = db.search("dark").unwrap();
        assert_eq!(results.len(), 2);
        assert!(results[0].name.contains("dark"));  // Best match first
    }
}
```

### 3.3. TypeScript Tests (Vitest)

```typescript
// Example: Component state test
describe('ChannelStrip', () => {
    it('should update meter level on audio data', () => {
        const strip = new ChannelStrip();
        
        strip.setVolume(-6.0);
        expect(strip.volume).toBe(-6.0);
        
        strip.setMeterLevel(-12.0);
        expect(strip.meterLevel).toBe(-12.0);
    });
    
    it('should clamp volume to valid range', () => {
        const strip = new ChannelStrip();
        
        strip.setVolume(30.0);  // Above max
        expect(strip.volume).toBe(24.0);  // Clamped
    });
});
```

---

## 4. Integration Testing

### 4.1. Module Boundary Tests

```cpp
// Test audio engine + plugin host integration
TEST_CASE("Audio graph processes with CLAP plugin", "[integration]") {
    AudioEngine audio;
    PluginHost plugins;
    
    // Setup: track with native CLAP plugin
    auto track_id = audio.create_track();
    auto plugin = plugins.load_plugin("aria.test_gain");
    audio.add_plugin_to_track(track_id, plugin);
    
    // Process audio through the chain
    AudioBuffer input(2, 128);
    input.channel(0).fill(0.5f);
    
    AudioBuffer output(2, 128);
    audio.process_track(track_id, input, output);
    
    // Gain plugin at default should pass audio through
    REQUIRE(output.channel(0)[0] == Approx(0.5f).margin(0.01));
}
```

### 4.2. Project Roundtrip

```cpp
TEST_CASE("Project save/load produces identical state", "[integration]") {
    // Create project with full state
    Project original;
    auto track = original.create_track("Test", TrackType::Audio);
    auto clip = original.create_midi_clip(track, 0, 960);
    clip->add_note(60, 0, 240, 100);
    
    // Save to buffer
    std::vector<uint8_t> data;
    ProjectSerializer serializer;
    serializer.save_to_buffer(original, data);
    
    // Load from buffer
    Project loaded;
    serializer.load_from_buffer(loaded, data);
    
    // Compare
    REQUIRE(loaded.track_count() == 1);
    REQUIRE(loaded.track(0).name() == "Test");
    REQUIRE(loaded.track(0).clip_count() == 1);
    auto loaded_clip = loaded.track(0).clip(0);
    REQUIRE(loaded_clip->note_count() == 1);
    REQUIRE(loaded_clip->note(0).pitch == 60);
}
```

---

## 5. Audio Testing

### 5.1. Audio Test Harness

```cpp
class AudioTestHarness {
public:
    // Generate test tone
    static AudioBuffer generate_sine(double frequency, double sample_rate,
                                      uint32_t frames, double amplitude = 0.5);
    
    // Generate impulse
    static AudioBuffer generate_impulse(uint32_t frames, uint32_t position = 0);
    
    // Generate noise
    static AudioBuffer generate_noise(uint32_t frames, double amplitude = 0.5);
    
    // Compare two buffers (sample-exact)
    static bool buffers_equal(const AudioBuffer& a, const AudioBuffer& b,
                               double epsilon = 1e-6);
    
    // Measure latency between input and output
    static uint32_t measure_latency(const AudioBuffer& input,
                                     const AudioBuffer& output);
    
    // Measure frequency response
    static std::vector<double> measure_frequency_response(
        std::function<void(AudioBuffer&)> processor,
        double sample_rate);
    
    // THD+N measurement
    static double measure_thdn(std::function<void(AudioBuffer&)> processor,
                                double frequency, double sample_rate);
};
```

### 5.2. Audio Tests

```cpp
TEST_CASE("Audio engine latency meets target", "[audio][performance]") {
    AudioEngine engine;
    engine.init({.sample_rate = 48000, .buffer_size = 64});
    
    // Measure round-trip latency
    auto input = AudioTestHarness::generate_impulse(48000);
    AudioBuffer output(2, 48000);
    
    engine.process(input, output);
    
    auto latency = AudioTestHarness::measure_latency(input, output);
    REQUIRE(latency <= 64);  // Must not exceed one buffer
}

TEST_CASE("DSP processor produces no distortion at 0dB", "[audio][dsp]") {
    GainProcessor gain;
    gain.set_gain(0.0);  // Unity
    
    auto input = AudioTestHarness::generate_sine(440, 48000, 48000, 1.0);
    AudioBuffer output(2, 48000);
    
    gain.process(input, output);
    
    auto thdn = AudioTestHarness::measure_thdn(
        [&](AudioBuffer& buf) { gain.process(buf, buf); },
        440, 48000);
    REQUIRE(thdn < 0.001);  // < 0.1% THD+N
}
```

---

## 6. Plugin Testing

### 6.1. Test Plugins

```cpp
// Test suite includes minimal CLAP/VST3 test plugins:
// - test_gain: Simple gain plugin
// - test_delay: 100-sample delay
// - test_noise: Noise generator
// - test_crash: Crashes on demand (for sandbox testing)
// - test_hang: Hangs on process (for timeout testing)
// - test_slow: Takes 10ms to process (for overload testing)

TEST_CASE("Plugin crash isolation", "[plugin][safety]") {
    PluginHost host;
    auto crash_plugin = host.load_plugin("test_crash");
    
    crash_plugin->set_parameter("crash_on_process", 1.0);
    
    // Plugin crash should not crash the host
    AudioBuffer input(2, 128), output(2, 128);
    auto result = host.process_safe(crash_plugin, input, output);
    
    REQUIRE(result == ARIA_RESULT_PLUGIN_CRASH);
    REQUIRE(host.is_alive());  // Host is still running
}
```

---

## 7. UI Testing

### 7.1. State Logic Tests

```typescript
// UI tests focus on state logic, not pixel-perfect rendering:

describe('TransportBar', () => {
    it('dispatches play command on click', () => {
        const transport = new TransportBar();
        const playSpy = vi.fn();
        
        transport.onPlay = playSpy;
        transport.clickPlayButton();
        
        expect(playSpy).toHaveBeenCalled();
    });
    
    it('shows recording indicator when armed', () => {
        const transport = new TransportBar();
        
        transport.setRecording(true);
        expect(transport.recordButton.color).toBe('#FF0000');
    });
});
```

### 7.2. Visual Regression Tests

```typescript
// GPU-rendered visual tests (future):
// 1. Render component to offscreen buffer
// 2. Compare with reference screenshot
// 3. Pixel-diff threshold: < 0.1% difference allowed
// 4. Run on CI with headless GPU (SwiftShader)

// Example (conceptual):
test('Button renders correctly', async () => {
    const button = new Button({ label: 'Click Me', variant: 'primary' });
    const pixels = await renderToBuffer(button, 100, 40);
    const diff = await compareWithReference('button-primary.png', pixels);
    expect(diff.percentDiff).toBeLessThan(0.1);
});
```

---

## 8. Performance Testing

### 8.1. Benchmarks

```cpp
BENCHMARK("Audio graph - 100 tracks", "[performance]") {
    AudioEngine engine;
    engine.init({.sample_rate = 48000, .buffer_size = 128});
    
    // Add 100 tracks with gain plugins
    for (int i = 0; i < 100; i++) {
        auto track = engine.create_track();
        auto plugin = engine.load_plugin("aria.test_gain");
        engine.add_plugin_to_track(track, plugin);
    }
    
    AudioBuffer input(2, 128);
    AudioBuffer output(2, 128);
    
    BENCHMARK_ADVANCED("process 100 tracks")(auto meter) {
        meter.measure([&]() {
            engine.process(input, output);
        });
    };
    
    // Expected: < 1ms for 100 tracks at 128 buffer
    REQUIRE(meter.mean() < 1'000);  // Microseconds
}

BENCHMARK("FPS - piano roll with 10k notes", "[performance]") {
    PianoRollRenderer renderer;
    std::vector<NoteInstance> notes(10000);
    
    renderer.update_note_data(notes);
    
    BENCHMARK_ADVANCED("render 10k notes")(auto meter) {
        meter.measure([&]() {
            renderer.render(command_buffer, viewport);
        });
    };
    
    // Expected: < 2ms GPU time for 10k notes
    REQUIRE(meter.mean() < 2'000);  // Microseconds
}
```

### 8.2. Performance Regression Thresholds

| Benchmark | Baseline | Threshold |
|---|---|---|
| Audio graph (100 tracks) | 0.5ms | 1.0ms (+100%) |
| DSP Biquad filter | 0.1μs/sample | 0.2μs/sample |
| FFT 2048 | 6μs | 12μs |
| Project save (100 tracks) | 50ms | 100ms |
| Plugin scan (100 plugins) | 2s | 5s |
| UI frame (typical) | 2ms | 5ms |
| Waveform render (10min) | 10ms | 25ms |
| MIDI playback (100 tracks) | 0.1ms | 0.3ms |

---

## 9. Regression Testing

### 9.1. Regression Test Suite

```cpp
// Every bug fix includes a regression test:
//
// Bug #1234: Compressor produces click at threshold crossing
// 
// Regression test:
TEST_CASE("No click at compressor threshold crossing - Bug #1234", "[regression]") {
    Compressor comp;
    comp.set_threshold(-20);
    comp.set_ratio(4);
    comp.set_attack(1);
    comp.set_release(50);
    
    // Signal that crosses threshold abruptly
    std::vector<float> input(48000);
    for (int i = 0; i < 24000; i++) input[i] = 0.01;    // Below threshold
    for (int i = 24000; i < 48000; i++) input[i] = 0.5;  // Above threshold
    
    std::vector<float> output(48000);
    comp.process(input.data(), output.data(), 48000);
    
    // Check for click (sample-to-sample discontinuity)
    for (int i = 1; i < 48000; i++) {
        float diff = std::abs(output[i] - output[i-1]);
        REQUIRE(diff < 0.01);  // No sudden jumps
    }
}
```

---

## 10. CI/CD Integration

### 10.1. CI Pipeline

```yaml
# GitHub Actions CI pipeline
jobs:
  unit-tests:
    runs-on: [windows-latest, macos-latest, ubuntu-latest]
    steps:
      - uses: actions/checkout@v4
      - run: cmake --build build --target aria_unit_tests
      - run: ./build/aria_unit_tests --verbosity high
      
  integration-tests:
    runs-on: [windows-latest, macos-latest]
    needs: unit-tests
    steps:
      - run: ./build/aria_integration_tests
      
  audio-tests:
    runs-on: [windows-latest]
    needs: unit-tests
    steps:
      - run: ./build/aria_audio_tests --sample-rate 48000
      - run: ./build/aria_audio_tests --sample-rate 96000
      
  performance-tests:
    runs-on: [windows-latest]
    needs: unit-tests
    steps:
      - run: ./build/aria_performance_tests
      - name: Check performance regression
        run: python scripts/check_perf_regression.py
```

---

## 11. Test Infrastructure

### 11.1. Test Utilities

```cpp
// Shared test utilities in test_common/:

// - TestAudioEngine: Minimal audio engine for testing
// - TestPluginHost: Plugin host with mock plugins
// - TestProjectBuilder: Builder pattern for test projects
// - AudioComparer: Sample-exact and perceptual comparison
// - MidiEventBuilder: Create MIDI event sequences
// - RandomNoteGenerator: Generate random note patterns
// - TempDirManager: RAII temporary directory
// - MockFileSystem: In-memory file system
```

---

## 12. Coverage Targets

| Module | Line Coverage | Branch Coverage | Notes |
|---|---|---|---|
| Audio Engine | 95% | 90% | Critical |
| MIDI Engine | 90% | 85% | Critical |
| DSP Library | 95% | 92% | Numerical correctness |
| Plugin Host | 85% | 80% | Sandboxing edge cases |
| Automation | 90% | 85% | Modulation paths |
| Project Format | 95% | 90% | Data integrity |
| Track Model | 90% | 85% | |
| Mixer | 85% | 80% | |
| Database (Rust) | 90% | 85% | |
| File Indexing | 85% | 80% | |
| Lua Scripting | 80% | 75% | |
| UI Components | 70% | 65% | Lower priority |
| **Overall** | **85%** | **80%** | |

---

## 13. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-TS-001 | Test Pyramid & Strategy | 🔲 Pending |
| RFC-TS-002 | Catch2 Test Framework Setup | 🔲 Pending |
| RFC-TS-003 | Rust Test Framework (cargo test) | 🔲 Pending |
| RFC-TS-004 | TypeScript Test Framework (Vitest) | 🔲 Pending |
| RFC-TS-005 | Audio Test Harness | 🔲 Pending |
| RFC-TS-006 | Audio Perceptual Comparison | 🔲 Pending |
| RFC-TS-007 | Test Plugins (gain, crash, hang, slow) | 🔲 Pending |
| RFC-TS-008 | Plugin Crash Isolation Tests | 🔲 Pending |
| RFC-TS-009 | UI State Logic Tests | 🔲 Pending |
| RFC-TS-010 | Visual Regression Tests | 🔲 Pending |
| RFC-TS-011 | Performance Benchmarks | 🔲 Pending |
| RFC-TS-012 | Performance Regression Detection | 🔲 Pending |
| RFC-TS-013 | Regression Test Policy | 🔲 Pending |
| RFC-TS-014 | CI Pipeline Configuration | 🔲 Pending |
| RFC-TS-015 | Coverage Reporting | 🔲 Pending |
| RFC-TS-016 | Fuzz Testing (Audio, MIDI, Project) | 🔲 Pending |
| RFC-TS-017 | Long-Run Stability Tests | 🔲 Pending |
| RFC-TS-018 | Plugin Compatibility Matrix | 🔲 Pending |

---

## Appendix A: Test Command Reference

```bash
# Run all tests
./build/aria_tests

# Run specific test suite
./build/aria_tests [audio]
./build/aria_tests [dsp]
./build/aria_tests [plugin]
./build/aria_tests [integration]

# Run with verbosity
./build/aria_tests --verbosity high

# Run performance benchmarks
./build/aria_tests [performance] --benchmark

# Run with sanitizers
./build/aria_tests --sanitizer address
./build/aria_tests --sanitizer thread
./build/aria_tests --sanitizer undefined

# Run with coverage
./build/aria_tests --coverage

# Rust tests
cargo test --package aria_database

# TypeScript tests
cd ui && npx vitest run
```
