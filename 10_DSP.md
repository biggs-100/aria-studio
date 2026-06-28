# ARIA DAW — DSP Engine Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: C++20/23 with SIMD (SSE/AVX/NEON)
> **Real-Time Safety**: Strict — no allocations

---

## Table of Contents

1. [Overview](#1-overview)
2. [SIMD Abstraction](#2-simd-abstraction)
3. [Core Primitives](#3-core-primitives)
4. [Filters](#4-filters)
5. [EQ](#5-eq)
6. [Dynamics](#6-dynamics)
7. [Time-Based Effects](#7-time-based-effects)
8. [Modulation Effects](#8-modulation-effects)
9. [Reverb](#9-reverb)
10. [Distortion & Saturation](#10-distortion--saturation)
11. [Utility Processors](#11-utility-processors)
12. [Analysis](#12-analysis)
13. [Time Stretch & Pitch Shift](#13-time-stretch--pitch-shift)
14. [FFT & Spectral Processing](#14-fft--spectral-processing)
15. [API Reference](#15-api-reference)
16. [RFC Index](#16-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The DSP Engine provides all built-in audio processing for ARIA DAW. It is a library of real-time safe, SIMD-accelerated signal processing primitives and effects that form the foundation of all audio processing in the application.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| SIMD acceleration | All hot paths (SSE2+, AVX2+, NEON) |
| Real-time safety | Zero allocations in processing |
| Precision | 64-bit float internal, 32-bit float I/O |
| Modulation | All parameters modulatable |
| Latency | Minimum possible per algorithm |

### 1.3. DSP Module Map

```
DSP Engine
├── Core
│   ├── SIMD Abstraction
│   ├── Buffer Operations
│   ├── Math Utilities
│   └── Denormal Protection
├── Filters
│   ├── Biquad (all types)
│   ├── State Variable Filter
│   ├── Formant Filter
│   └── Comb Filter
├── EQ
│   ├── Parametric EQ (8 bands)
│   ├── Graphic EQ
│   └── Linear Phase EQ
├── Dynamics
│   ├── Compressor
│   ├── Limiter
│   ├── Gate/Expander
│   ├── De-esser
│   └── Multiband Compressor
├── Time Effects
│   ├── Delay (mono/stereo/ping-pong)
│   ├── Chorus
│   ├── Flanger
│   ├── Phaser
│   └── Reverb (algorithmic + convolution)
├── Modulation
│   ├── Tremolo
│   ├── Auto-pan
│   ├── Ring Modulator
│   └── Frequency Shifter
├── Distortion
│   ├── Waveshaper (multiple curves)
│   ├── Tape Saturation
│   ├── Tube Saturation
│   └── Bit Crusher
├── Utility
│   ├── Gain
│   ├── Pan
│   ├── Meter
│   ├── Phase Invert
│   ├── Stereo Tools
│   └── Tone Generator
└── Analysis
    ├── FFT
    ├── Pitch Detection
    ├── Transient Detection
    ├── Onset Detection
    └── Key Detection
```

---

## 2. SIMD Abstraction

### 2.1. SIMD Dispatcher

```cpp
class SIMD {
public:
    enum class Level {
        Scalar,     // No SIMD (fallback)
        SSE2,       // 128-bit, 4 floats
        SSE4_1,     // 128-bit with blend
        AVX,        // 256-bit, 8 floats
        AVX2,       // 256-bit with FMA
        AVX512,     // 512-bit, 16 floats
        NEON,       // 128-bit ARM
        NEON64      // 128-bit ARMv8
    };
    
    // Detect at runtime
    static Level detect();
    
    // Convert to human-readable
    static const char* name(Level level);
    
    // Minimum guaranteed level
    static constexpr Level MINIMUM = Level::SSE2;
    
    // Compile-time dispatch helper
    template<typename... Funcs>
    static auto dispatch(Level level, Funcs... funcs) -> decltype(auto);
    
    // Buffer operations (dispatched)
    using BufferOp = void(*)(float*, const float*, uint32_t);
    
    static BufferOp get_add_func();       // buffer += src
    static BufferOp get_multiply_func();  // buffer *= src
    static BufferOp get_copy_func();      // buffer = src
    static BufferOp get_clear_func();     // buffer = 0
    
private:
    static Level detected_;
};
```

### 2.2. SIMD Buffer Operations

```cpp
// All buffer operations have multiple implementations:
// scalar, SSE2, AVX, AVX2, AVX512, NEON

// Example: multiply buffer by gain
namespace simd_impl {

void multiply_gain_scalar(float* buffer, float gain, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i)
        buffer[i] *= gain;
}

#ifdef __AVX__
void multiply_gain_avx(float* buffer, float gain, uint32_t frames) {
    __m256 gain_vec = _mm256_set1_ps(gain);
    uint32_t i = 0;
    for (; i + 8 <= frames; i += 8) {
        __m256 data = _mm256_loadu_ps(buffer + i);
        data = _mm256_mul_ps(data, gain_vec);
        _mm256_storeu_ps(buffer + i, data);
    }
    // Remainder
    for (; i < frames; ++i)
        buffer[i] *= gain;
}
#endif

} // namespace simd_impl
```

### 2.3. Denormal Protection

```cpp
// Flush denormals to zero to prevent performance degradation
// on CPUs where denormals cause 10-100x slowdown

class DenormalProtection {
public:
    // Disable denormals for this thread
    static void enable_flush_to_zero();
    
    // Small epsilon to prevent denormals in recursive filters
    static constexpr float DENORMAL_EPSILON = 1e-25f;
    
    // Apply to feedback signals
    static float protect(float value) {
        // Flush to zero if value is denormal
        if (std::abs(value) < 1.17549435e-38f)
            return 0.0f;
        return value;
    }
};
```

---

## 3. Core Primitives

### 3.1. Buffer Math

```cpp
namespace DSP {

// Audio buffer operations (SIMD-accelerated)
void add(float* dst, const float* src, uint32_t frames);
void add_with_gain(float* dst, const float* src, float gain, uint32_t frames);
void copy(float* dst, const float* src, uint32_t frames);
void multiply(float* dst, float gain, uint32_t frames);
void multiply(float* dst, const float* src, uint32_t frames);
void scale_add(float* dst, const float* src, float scale, uint32_t frames);
void interleave(const float** channels, uint32_t num_channels,
                float* output, uint32_t frames);
void deinterleave(const float* input, float** channels,
                  uint32_t num_channels, uint32_t frames);
void mix(const float* a, const float* b, float* out,
         float amount, uint32_t frames);

// Math
float peak(const float* buffer, uint32_t frames);
float rms(const float* buffer, uint32_t frames);
float sum_squares(const float* buffer, uint32_t frames);
void apply_fade_in(float* buffer, uint32_t frames);
void apply_fade_out(float* buffer, uint32_t frames);

// Vector operations (SIMD)
void vector_add(float* result, const float* a, const float* b, uint32_t n);
void vector_mul(float* result, const float* a, const float* b, uint32_t n);
void vector_mac(float* acc, const float* a, float b, uint32_t n);
float vector_dot(const float* a, const float* b, uint32_t n);

} // namespace DSP
```

### 3.2. Audio Buffer

```cpp
class AudioBlock {
public:
    AudioBlock(uint32_t channels, uint32_t frames);
    ~AudioBlock() = default;
    
    // Access
    float* channel(uint32_t ch);
    const float* channel(uint32_t ch) const;
    uint32_t channels() const;
    uint32_t frames() const;
    
    // Operations
    void clear();
    void copy_from(const AudioBlock& src);
    void apply_gain(float gain_db);
    
    // SIMD operations
    void add(const AudioBlock& src);
    void multiply(const AudioBlock& src);
    
    // Non-copying split/join
    AudioBlock subblock(uint32_t start_frame, uint32_t length);
    
private:
    // Channel data is stored interleaved for cache efficiency
    // or planar for SIMD efficiency, depending on operation
    std::vector<float, AlignedAllocator<64>> data_;
    uint32_t channels_;
    uint32_t frames_;
};
```

---

## 4. Filters

### 4.1. Biquad Filter

```cpp
class BiquadFilter {
public:
    enum class Type {
        LowPass, HighPass, BandPass, Notch,
        AllPass, LowShelf, HighShelf, Peak,
        ResoLowPass   // Resonant low-pass (Moog-inspired)
    };
    
    BiquadFilter();
    
    // Configure
    void set_type(Type type);
    void set_cutoff(double frequency_hz);    // 20-20000 Hz
    void set_resonance(double q);            // 0.1 - 10
    void set_gain(double gain_db);           // -36 to +36 dB (shelf/peak)
    
    // Sample rate
    void set_sample_rate(double sample_rate);
    
    // Process
    void process(const float* input, float* output, uint32_t frames);
    void process_block(AudioBlock& block);
    
    // Reset state
    void reset();
    
    // Modulatable parameters
    void set_cutoff_modulated(double frequency_hz);  // Called from mod engine
    void set_resonance_modulated(double q);
    
    // Coefficients update
    void update_coefficients();

private:
    Type type_ = Type::LowPass;
    double sample_rate_ = 48000.0;
    
    // Target values
    double target_cutoff_ = 1000.0;
    double target_resonance_ = 0.707;
    double target_gain_ = 0.0;
    
    // Current modulated values (may differ from targets)
    double cutoff_ = 1000.0;
    double resonance_ = 0.707;
    
    // Coefficients
    double b0_, b1_, b2_, a1_, a2_;
    
    // State (for each channel)
    struct State {
        double x1 = 0.0, x2 = 0.0;
        double y1 = 0.0, y2 = 0.0;
    };
    std::vector<State> channel_states_;
    
    // Coefficient calculation (RBJ cookbook)
    void calculate_coefficients();
};
```

### 4.2. State Variable Filter

```cpp
class SVFilter {
public:
    // Simultaneous outputs: lowpass, highpass, bandpass, notch
    // Perfect for modulation — responds smoothly to cutoff changes
    
    SVFilter();
    
    void set_cutoff(double frequency_hz);
    void set_resonance(double q);
    void set_sample_rate(double sr);
    
    // Process: returns LP, HP, BP simultaneously
    struct FilterOutput {
        float lowpass;
        float highpass;
        float bandpass;
        float notch;
    };
    FilterOutput process(float input);
    
    void process_block(const AudioBlock& input, AudioBlock& output_lp,
                       AudioBlock& output_hp, AudioBlock& output_bp);
    
    void reset();

private:
    double sample_rate_;
    double cutoff_;
    double resonance_;
    
    // State
    double low_ = 0.0, high_ = 0.0, band_ = 0.0;
    double g_, R_;  // Frequency and damping coefficients
};
```

---

## 5. EQ

### 5.1. Parametric EQ

```cpp
class ParametricEQ {
public:
    ParametricEQ();
    
    // Up to 8 bands
    struct Band {
        enum Type { LowShelf, Peak, HighShelf, LowCut, HighCut, BandPass, Notch };
        Type type = Peak;
        double frequency = 1000.0;  // Hz
        double gain_db = 0.0;       // dB
        double q = 0.707;           // Quality factor
        bool active = true;
    };
    
    uint32_t add_band(const Band& band);
    void remove_band(uint32_t index);
    void modify_band(uint32_t index, const Band& band);
    void set_band_active(uint32_t index, bool active);
    void clear();
    uint32_t band_count() const;
    
    // Process
    void process(AudioBlock& block);
    
    // Bypass
    void set_bypass(bool bypass);
    bool is_bypassed() const;
    
    // Latency
    uint32_t latency() const { return 0; }  // Minimum phase = zero latency
    
    // Presets
    void load_preset(const EQPreset& preset);
    
private:
    std::vector<BiquadFilter> bands_;
    bool bypassed_ = false;
    double sample_rate_ = 48000.0;
};
```

### 5.2. Linear Phase EQ

```cpp
class LinearPhaseEQ {
public:
    LinearPhaseEQ();
    
    // Same band configuration as ParametricEQ
    // But uses FFT-based convolution for linear phase response
    // Latency = FFT size / 2
    
    void set_bands(const std::vector<ParametricEQ::Band>& bands);
    void set_fft_size(uint32_t size);  // 512 - 16384
    
    // Process
    void process(AudioBlock& block);
    
    // Latency (depends on FFT size)
    uint32_t latency() const;  // Returns FFT size / 2
    
    // Bypass
    void set_bypass(bool bypass);
    
private:
    std::vector<float> impulse_response_;
    uint32_t fft_size_ = 2048;
    bool bypassed_ = false;
    bool needs_update_ = true;
    
    // Convolution engine
    FFTConvolver convolver_;
    
    void update_impulse_response();
};
```

---

## 6. Dynamics

### 6.1. Compressor

```cpp
class Compressor {
public:
    Compressor();
    
    // Controls
    void set_threshold(double db);       // -60 to 0 dB
    void set_ratio(double ratio);        // 1:1 to 50:1
    void set_attack(double ms);          // 0.1 to 100 ms
    void set_release(double ms);         // 10 to 2000 ms
    void set_knee(double db);            // 0 to 12 dB (soft knee)
    void set_makeup_gain(double db);     // 0 to +24 dB
    void set_mix(double wet);            // 0.0-1.0 (dry/wet)
    
    // Detection mode
    enum class Detection {
        Peak,           // Peak detection (fast)
        RMS,            // RMS detection (smooth)
        Sidechain,      // External sidechain input
    };
    void set_detection(Detection mode);
    
    // Sidechain
    void set_sidechain_input(const AudioBlock* input);
    
    // Auto makeup
    void set_auto_makeup(bool enabled);
    bool auto_makeup() const;
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    
    // Metering
    double gain_reduction() const;  // Current GR in dB
    
    // Bypass
    void set_bypass(bool bypass);
    bool is_bypassed() const;
    
    // Reset
    void reset();

private:
    // Parameters
    double threshold_db_ = -18.0;
    double ratio_ = 4.0;
    double attack_ms_ = 5.0;
    double release_ms_ = 100.0;
    double knee_db_ = 6.0;
    double makeup_gain_db_ = 0.0;
    double mix_ = 1.0;
    Detection detection_ = Detection::RMS;
    bool auto_makeup_ = false;
    bool bypassed_ = false;
    
    // Envelope follower state
    double envelope_ = 0.0;
    double gain_reduction_ = 0.0;
    
    // Sidechain
    const AudioBlock* sidechain_input_ = nullptr;
    
    // Processing
    double compute_gain(double level_db) const;
    double apply_knee(double diff_db) const;
    
    // Coefficients
    double attack_coeff_;
    double release_coeff_;
    void update_coefficients();
};
```

### 6.2. Multiband Compressor

```cpp
class MultibandCompressor {
public:
    MultibandCompressor();
    
    // Band configuration (3 or 4 bands)
    struct BandConfig {
        double crossover_low = 200.0;    // Hz
        double crossover_mid = 2000.0;   // Hz
        // Band 1: 20 - crossover_low
        // Band 2: crossover_low - crossover_mid
        // Band 3: crossover_mid - 20kHz
        // (Optional Band 4 with additional crossover)
    };
    
    void set_band_config(const BandConfig& config);
    void set_band_parameters(uint32_t band,
                             double threshold, double ratio,
                             double attack, double release);
    
    // Crossovers
    enum class CrossoverType {
        LinkwitzRiley_12,   // 12 dB/oct
        LinkwitzRiley_24,   // 24 dB/oct (default)
        LinkwitzRiley_48    // 48 dB/oct
    };
    void set_crossover_type(CrossoverType type);
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    
    // Solo band
    void set_solo_band(uint32_t band);  // -1 = no solo
    
private:
    BandConfig config_;
    CrossoverType crossover_ = CrossoverType::LinkwitzRiley_24;
    
    // Crossover filters
    std::vector<BiquadFilter> crossover_filters_;
    
    // Per-band compressors
    std::vector<std::unique_ptr<Compressor>> band_compressors_;
    
    // Crossover network
    void update_crossovers();
};
```

### 6.3. Limiter

```cpp
class BrickwallLimiter {
public:
    BrickwallLimiter();
    
    void set_threshold(double db);       // -6 to 0 dB
    void set_ceiling(double db);        // -6 to 0 dB
    void set_release(double ms);        // 10 to 500 ms
    void set_style(LimiterStyle style);
    
    enum class LimiterStyle {
        Modern,      // Aggressive, transparent
        Classic,     // Softer clipping character
        Safe         // Conservative, high headroom
    };
    
    // True peak mode (oversamples 4x)
    void set_true_peak_mode(bool enabled);
    
    // Process
    void process(AudioBlock& block);
    
    // Metering
    double gain_reduction() const;
    bool is_limiting() const;
    
    // Bypass
    void set_bypass(bool bypass);

private:
    double threshold_db_ = -1.0;
    double ceiling_db_ = -0.3;
    double release_ms_ = 100.0;
    LimiterStyle style_ = LimiterStyle::Modern;
    bool true_peak_ = true;
    bool bypassed_ = false;
    
    // Lookahead delay line
    std::vector<float> delay_buffer_;
    uint32_t delay_write_pos_ = 0;
    uint32_t lookahead_samples_ = 5;
    
    // Gain computer
    double gain_reduction_ = 0.0;
    double envelope_ = 0.0;
};
```

---

## 7. Time-Based Effects

### 7.1. Delay

```cpp
class Delay {
public:
    Delay();
    
    void set_time(double ms);             // 1-2000 ms
    void set_time_sync(double note_value); // 1/32 to 4/1
    bool is_time_synced() const;
    
    void set_feedback(double percent);    // 0-100%
    void set_dry_wet(double mix);         // 0.0-1.0
    void set_lowpass_cutoff(double hz);   // Feedback filter
    void set_highpass_cutoff(double hz);
    
    enum class Mode {
        Mono,           // Mono delay
        Stereo,         // Independent L/R
        PingPong,       // Alternating L/R
        CrossFeed       // Cross-feed between channels
    };
    void set_mode(Mode mode);
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    
    // Reset
    void reset();

private:
    double sample_rate_;
    uint32_t max_delay_samples_ = 96000;  // 2s at 48kHz
    
    // Delay line
    std::vector<float, AlignedAllocator<64>> delay_line_left_;
    std::vector<float, AlignedAllocator<64>> delay_line_right_;
    uint32_t write_pos_ = 0;
    
    // Parameters
    double delay_ms_ = 250.0;
    double delay_note_ = 1.0 / 8;  // 8th note
    bool time_synced_ = false;
    double feedback_ = 0.3;
    double mix_ = 0.5;
    Mode mode_ = Mode::PingPong;
    
    // Feedback filter
    BiquadFilter feedback_lp_;
    BiquadFilter feedback_hp_;
};
```

### 7.2. Chorus

```cpp
class Chorus {
public:
    Chorus();
    
    void set_rate(double hz);            // 0.1-10 Hz
    void set_depth(double percent);      // 0-100%
    void set_delay(double ms);           // 5-30 ms
    void set_feedback(double percent);   // 0-100%
    void set_dry_wet(double mix);        // 0.0-1.0
    
    // Stereo spread
    void set_stereo_spread(double phase); // 0-360 degrees
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    
    void reset();
    
private:
    // Two modulated delay lines (L/R)
    ModulatedDelay delay_left_;
    ModulatedDelay delay_right_;
    
    // LFO for modulation
    LFO lfo_;
    
    double rate_ = 0.5;
    double depth_ = 0.5;
    double delay_ms_ = 15.0;
    double feedback_ = 0.2;
    double mix_ = 0.5;
    double stereo_phase_ = 180.0;  // L/R out of phase
};
```

### 7.3. Phaser

```cpp
class Phaser {
public:
    Phaser();
    
    // Controls
    void set_rate(double hz);            // 0.1-10 Hz
    void set_depth(double percent);      // 0-100%
    void set_feedback(double percent);   // 0-100%
    void set_stages(uint32_t count);     // 2-24 (default 6)
    void set_center_frequency(double hz); // 200-2000 Hz
    void set_dry_wet(double mix);
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    void reset();
    
private:
    uint32_t stages_ = 6;
    double rate_ = 0.4;
    double depth_ = 0.7;
    double feedback_ = 0.4;
    double center_freq_ = 800.0;
    double mix_ = 0.5;
    
    // All-pass filter chain
    std::vector<BiquadFilter> allpass_filters_;
    double lfo_phase_ = 0.0;
};
```

---

## 8. Modulation Effects

### 8.1. Tremolo

```cpp
class Tremolo {
public:
    Tremolo();
    
    void set_rate(double hz);            // 0.1-20 Hz
    void set_rate_sync(double note_value);
    void set_depth(double percent);      // 0-100%
    void set_shape(double shape);        // 0.0 (saw) - 1.0 (square), 0.5 (sine)
    void set_stereo_phase(double phase); // 0-360 degrees
    
    void process(const AudioBlock& input, AudioBlock& output);
    void reset();
    
private:
    LFO lfo_;
    double rate_ = 5.0;
    double depth_ = 0.7;
    double shape_ = 0.5;   // Sine default
    double stereo_phase_ = 0.0;
};
```

### 8.2. Ring Modulator

```cpp
class RingModulator {
public:
    RingModulator();
    
    void set_frequency(double hz);       // 20-5000 Hz
    void set_frequency_sync(double note_value);
    void set_depth(double percent);      // 0-100%
    void set_dry_wet(double mix);
    
    // Waveform
    enum class Waveform { Sine, Square, Saw, Triangle, Noise };
    void set_waveform(Waveform wf);
    
    void process(const AudioBlock& input, AudioBlock& output);
    void reset();
    
private:
    double frequency_ = 440.0;
    double depth_ = 0.5;
    double mix_ = 1.0;
    Waveform waveform_ = Waveform::Sine;
    double phase_ = 0.0;
};
```

---

## 9. Reverb

### 9.1. Algorithmic Reverb

```cpp
class Reverb {
public:
    Reverb();
    
    // Controls
    void set_room_size(double percent);   // 0-100%
    void set_decay(double seconds);       // 0.1-20s
    void set_pre_delay(double ms);        // 0-200ms
    void set_diffusion(double percent);   // 0-100%
    void set_damping(double percent);     // 0-100% (high-frequency rolloff)
    void set_low_cut(double hz);          // 20-500 Hz
    void set_high_cut(double hz);         // 1000-20000 Hz
    void set_dry_wet(double mix);         // 0.0-1.0
    
    // Algorithm type
    enum class Algorithm {
        Hall,           // Large, smooth
        Room,           // Smaller, more defined
        Chamber,        // Bright, dense
        Plate,          // Smooth, bright
        Spring,         // Spring reverb character
        Cathedral,      // Very large, ethereal
        Ambience        // Short, subtle space
    };
    void set_algorithm(Algorithm algo);
    
    // Modulation (for chorus/ movement in reverb tail)
    void set_modulation_rate(double hz);
    void set_modulation_depth(double percent);
    
    // Early reflections
    void set_early_reflections(double level);  // 0-100%
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    void reset();
    
private:
    Algorithm algorithm_ = Algorithm::Hall;
    
    // Parameters
    double room_size_ = 0.7;
    double decay_ = 3.0;
    double pre_delay_ms_ = 30.0;
    double diffusion_ = 0.6;
    double damping_ = 0.5;
    double low_cut_ = 100.0;
    double high_cut_ = 12000.0;
    double mix_ = 0.3;
    double modulation_rate_ = 0.2;
    double modulation_depth_ = 0.1;
    double early_reflections_ = 0.5;
    
    // Comb filters (parallel)
    std::vector<CombFilter> combs_;
    
    // All-pass filters (series)
    std::vector<BiquadFilter> allpasses_;
    
    // Delay line for pre-delay
    DelayLine pre_delay_;
    
    // Damping filter
    BiquadFilter damping_filter_;
    
    // Modulation
    LFO modulation_lfo_;
};
```

### 9.2. Convolution Reverb

```cpp
class ConvolutionReverb {
public:
    ConvolutionReverb();
    
    // Load impulse response
    bool load_impulse(const std::string& file_path);
    bool load_impulse(const float* ir_data, uint32_t length, uint32_t channels);
    
    void set_dry_wet(double mix);
    void set_length(double percent);   // Trim/expand IR
    void set_gain(double db);
    
    // Partitioned convolution for low latency
    // Uses FFT for efficiency
    
    void process(const AudioBlock& input, AudioBlock& output);
    void reset();
    
    // Info
    std::string current_ir_name() const;
    double ir_length_ms() const;
    
private:
    // Impulse response data
    std::vector<float> ir_left_;
    std::vector<float> ir_right_;
    std::string ir_name_;
    bool ir_loaded_ = false;
    
    // Partitioned convolution
    static constexpr uint32_t PARTITION_SIZE = 512;
    struct Partition {
        std::vector<float> fft_data;  // Pre-transformed
    };
    std::vector<Partition> partitions_;
    
    // Processing state
    std::vector<float> input_buffer_;
    uint32_t input_pos_ = 0;
    
    double mix_ = 0.3;
    double gain_ = 0.0;
    
    // Build partitions from IR
    void build_partitions();
};
```

---

## 10. Distortion & Saturation

### 10.1. Waveshaper

```cpp
class Waveshaper {
public:
    Waveshaper();
    
    enum class Curve {
        SoftClip,       // Arctangent soft clipping
        HardClip,       // Hard clip at threshold
        Sine,           // Sine folding
        Tanh,           // Hyperbolic tangent (warm)
        Cubic,          // Cubic distortion
        Exponential,    // Asymmetric tube-like
        Logarithmic,    // Compression-like
        Asymmetric,     // Asymmetric (even harmonics)
        Rectify,        // Half/full wave rectification
        Foldback,       // Wave folding (digital)
        Custom          // User-defined curve via Lua
    };
    
    void set_curve(Curve type);
    void set_drive(double percent);      // 0-100%
    void set_output(double db);          // -24 to +24 dB
    void set_mix(double wet);            // 0.0-1.0
    
    // Pre/Post filtering
    void set_low_cut(double hz);
    void set_high_cut(double hz);
    
    // Oversampling
    enum class Oversample {
        None, 1x, 2x, 4x, 8x
    };
    void set_oversampling(Oversample os);
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    
    // Custom curve (for Lua scripting)
    void set_custom_curve(std::function<float(float)> fn);
    
private:
    Curve curve_ = Curve::SoftClip;
    double drive_ = 0.5;
    double output_db_ = 0.0;
    double mix_ = 1.0;
    Oversample oversample_ = Oversample::None;
    
    // Shape functions
    static float soft_clip(float x, double drive);
    static float hard_clip(float x, double drive);
    static float tanh_curve(float x, double drive);
    
    // Oversampling state
    uint32_t oversample_factor() const;
    std::vector<float> upsample_buffer_;
    std::vector<float> downsample_buffer_;
};
```

### 10.2. Bit Crusher

```cpp
class BitCrusher {
public:
    BitCrusher();
    
    void set_bit_depth(uint32_t bits);     // 1-24 bits
    void set_sample_rate_reduction(uint32_t factor); // 1-32x reduction
    void set_mix(double wet);
    
    // Noise shaping
    void set_noise_shaping(bool enabled);
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    
private:
    uint32_t bit_depth_ = 8;
    uint32_t rate_reduction_ = 4;
    double mix_ = 1.0;
    bool noise_shaping_ = false;
    
    // Rate reduction state
    uint32_t sample_counter_ = 0;
    float last_sample_left_ = 0.0;
    float last_sample_right_ = 0.0;
    
    // Noise shaping filter
    BiquadFilter noise_shaper_;
};
```

---

## 11. Utility Processors

### 11.1. Stereo Tools

```cpp
class StereoProcessor {
public:
    StereoProcessor();
    
    void set_width(double width);         // 0.0 (mono) to 2.0 (wide)
    void set_balance(double balance);     // -1.0 (L) to +1.0 (R)
    void set_mid_side_mode(bool enabled);
    
    // Mid/Side processing
    void set_mid_gain(double db);
    void set_side_gain(double db);
    void set_mid_eq(ParametricEQ& eq);     // EQ only mid channel
    void set_side_eq(ParametricEQ& eq);    // EQ only side channel
    
    // Correlation meter
    double phase_correlation() const;
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    
private:
    double width_ = 1.0;
    double balance_ = 0.0;
    bool mid_side_ = false;
    double mid_gain_ = 0.0;
    double side_gain_ = 0.0;
};
```

### 11.2. Tone Generator

```cpp
class ToneGenerator {
public:
    ToneGenerator();
    
    enum class Waveform {
        Sine, Square, Saw, Triangle,
        Noise, Pulse, WhiteNoise, PinkNoise
    };
    
    void set_waveform(Waveform wf);
    void set_frequency(double hz);       // 20-20000 Hz
    void set_amplitude(double db);       // -60 to 0 dB
    void set_pulse_width(double width);  // 0.0-1.0 (for pulse wave)
    
    // Test tones
    void set_sweep(double start_hz, double end_hz, double duration_s);
    void set_impulse();                  // Single sample impulse
    void set_silence();                  // Digital black
    
    // Process
    void process(AudioBlock& output);
    
    void reset();
    
private:
    Waveform waveform_ = Waveform::Sine;
    double frequency_ = 440.0;
    double amplitude_db_ = -18.0;
    double pulse_width_ = 0.5;
    
    // Sweep state
    bool sweeping_ = false;
    double sweep_start_, sweep_end_, sweep_duration_;
    double sweep_time_ = 0.0;
    
    // Phase
    double phase_ = 0.0;
    
    // Noise state (Xorshift)
    uint32_t noise_state_ = 123456789;
};
```

---

## 12. Analysis

### 12.1. FFT

```cpp
class FFT {
public:
    FFT();
    
    void init(uint32_t size);  // Must be power of 2
    void free();
    
    // Forward transform (real → complex)
    void forward(const float* real_input,
                 std::complex<float>* complex_output);
    
    // Inverse transform (complex → real)
    void inverse(const std::complex<float>* complex_input,
                 float* real_output);
    
    // Magnitude spectrum
    void magnitude_spectrum(const std::complex<float>* fft_data,
                            float* magnitudes,
                            bool normalize = true);
    
    // Phase spectrum
    void phase_spectrum(const std::complex<float>* fft_data,
                        float* phases);
    
    // Helper: frequency from bin index
    static float bin_to_frequency(uint32_t bin, uint32_t fft_size,
                                   double sample_rate);
    
    // Helper: window function
    enum class Window {
        Rectangular, Hanning, Hamming, Blackman,
        BlackmanHarris, Kaiser, FlatTop
    };
    static void apply_window(float* data, uint32_t size, Window window);
    
    uint32_t size() const;
    uint32_t bin_count() const;

private:
    uint32_t fft_size_ = 0;
    uint32_t bin_count_ = 0;
    
    // FFT plan (pffft or custom)
    void* plan_ = nullptr;
};
```

### 12.2. Pitch Detection

```cpp
class PitchDetector {
public:
    PitchDetector();
    
    // Analyze buffer and return detected pitch
    struct PitchResult {
        double frequency_hz = 0.0;
        double confidence = 0.0;      // 0.0-1.0
        std::string note_name;         // "C4", "A#3", etc.
        int midi_note = -1;            // MIDI note number (or -1 if unpitched)
        double cents_deviation = 0.0;  // Deviation from equal temperament
        bool is_pitched = false;
    };
    
    PitchResult detect(const float* buffer, uint32_t frames,
                       double sample_rate);
    
    // Algorithm selection
    enum class Algorithm {
        Autocorrelation,    // YIN algorithm
        Cepstrum,           // Cepstral analysis
        HPS,                // Harmonic product spectrum
        Combined            // Weighted combination
    };
    void set_algorithm(Algorithm algo);
    
    // Sensitivity
    void set_sensitivity(double percent);  // 0-100%

private:
    Algorithm algorithm_ = Algorithm::Combined;
    double sensitivity_ = 0.8;
    
    double yin_pitch(const float* buffer, uint32_t frames, double sr);
    double cepstrum_pitch(const float* buffer, uint32_t frames, double sr);
    double hps_pitch(const float* buffer, uint32_t frames, double sr);
    
    // Note name helper
    static const char* NOTE_NAMES[12];
};
```

### 12.3. Transient Detection

```cpp
class TransientDetector {
public:
    TransientDetector();
    
    // Detect transient onsets
    struct Transient {
        uint64_t sample_position;
        float strength;             // 0.0-1.0
        float peak_level;           // dB
    };
    
    std::vector<Transient> detect(const float* buffer, uint32_t frames,
                                   double sample_rate);
    
    // Sensitivity
    void set_sensitivity(double percent);
    
    // Minimum interval
    void set_min_interval_ms(double ms);   // 10-500ms
    
    // Algorithm
    enum class Algorithm {
        EnergyBased,     // Spectral flux / energy rise
        PhaseBased,      // Phase deviation
        Complex          // Combined approach
    };
    
private:
    double sensitivity_ = 0.7;
    double min_interval_ms_ = 30.0;
    Algorithm algorithm_ = Algorithm::EnergyBased;
    
    // Spectral flux calculation
    std::vector<float> prev_spectrum_;
    bool first_frame_ = true;
};
```

---

## 13. Time Stretch & Pitch Shift

### 13.1. Time Stretcher

```cpp
class TimeStretcher {
public:
    TimeStretcher();
    
    void set_stretch_ratio(double ratio);  // 0.5 (half speed) to 2.0 (double)
    void set_pitch_shift(int32_t cents);   // -2400 to +2400 cents (±2 octaves)
    void set_formant_preserve(bool preserve);
    
    // Quality
    enum class Quality {
        Fast,           // Minimal CPU, lower quality
        Good,           // Default quality
        Highest        // Best quality (offline)
    };
    void set_quality(Quality q);
    
    // Process
    void process(const float* input, uint32_t input_frames,
                 float* output, uint32_t& output_frames);
    
    void reset();
    
private:
    double stretch_ratio_ = 1.0;
    int32_t pitch_cents_ = 0;
    bool preserve_formants_ = false;
    Quality quality_ = Quality::Good;
    
    // Phase vocoder state
    uint32_t fft_size_ = 2048;
    uint32_t hop_size_ = 512;
    
    // Overlap-add buffers
    std::vector<float> synthesis_buffer_;
    uint32_t write_position_ = 0;
    
    // Frequency-domain processing
    FFT fft_;
    std::vector<std::complex<float>> prev_phase_;
};
```

### 13.2. Elastique-style Stretching

```cpp
// For high-quality time stretching (like Elastique or zplane):
// Uses a combination of:
// - Time-domain SOLA (Synchronous Overlap-Add)
// - Frequency-domain phase vocoding
// - Transient preservation (doesn't smear attacks)

class ElastiqueStretcher {
public:
    struct Config {
        double ratio = 1.0;           // Time stretch ratio
        int32_t pitch_cents = 0;      // Pitch shift
        bool preserve_formants = true;
        bool preserve_transients = true;
        Quality quality = Quality::High;
    };
    
    void process(const AudioBlock& input, AudioBlock& output);
    void reset();
    
    // Latency
    uint32_t latency() const;
    
private:
    // Transient detection → split signal into
    // transient and stationary parts
    // Process transient part with SOLA (no smearing)
    // Process stationary part with phase vocoder (better quality)
    // Recombine
    
    TransientDetector transient_detector_;
    SOLAProcessor sola_;
    PhaseVocoder pvoc_;
};
```

---

## 14. FFT & Spectral Processing

### 14.1. Spectral Processor

```cpp
class SpectralProcessor {
public:
    SpectralProcessor();
    
    // Process function (applied per-bin in frequency domain)
    using SpectralFunc = std::function<std::complex<float>(
        uint32_t bin, float frequency, std::complex<float> value)>;
    
    void set_process_function(SpectralFunc fn);
    
    // Pre-built spectral operations
    void set_filter(const std::vector<float>& frequency_response);
    void set_noise_gate(float threshold_db);
    void set_denoiser(float amount);
    
    // FFT size
    void set_fft_size(uint32_t size);
    void set_hop_size(uint32_t size);
    
    // Window
    void set_window(FFT::Window window);
    
    // Process
    void process(const AudioBlock& input, AudioBlock& output);
    
private:
    uint32_t fft_size_ = 2048;
    uint32_t hop_size_ = 512;
    FFT::Window window_ = FFT::Window::Hanning;
    
    FFT fft_;
    SpectralFunc process_func_;
    
    // Overlap-add
    std::vector<float> overlap_buffer_;
    
    // Analysis buffers
    std::vector<float> analysis_window_;
    std::vector<float> synthesis_window_;
};
```

---

## 15. API Reference

### 15.1. DSP Factory

```cpp
class DSPFactory {
public:
    // Create DSP processor by type
    template<typename T>
    static std::unique_ptr<T> create();
    
    // Create from string identifier
    static std::unique_ptr<DSPProcessor> create(const std::string& type_id);
    
    // List all registered processor types
    static std::vector<std::string> registered_types();
    
    // Register custom processor (for Lua scripts)
    static void register_type(const std::string& id,
                               std::function<DSPProcessor*()> factory);
};
```

### 15.2. DSP Processor Interface

```cpp
class DSPProcessor {
public:
    virtual ~DSPProcessor() = default;
    
    // Identification
    virtual std::string type_id() const = 0;
    virtual std::string name() const = 0;
    
    // Configuration
    virtual void set_sample_rate(double sample_rate);
    virtual void set_parameter(uint32_t index, double value);
    virtual double get_parameter(uint32_t index) const;
    virtual uint32_t parameter_count() const;
    virtual ParameterInfo parameter_info(uint32_t index) const;
    
    // Processing
    virtual void process(AudioBlock& block) = 0;
    virtual void process(const AudioBlock& input, AudioBlock& output);
    
    // State
    virtual void reset();
    virtual uint32_t latency() const;
    
    // Bypass
    virtual void set_bypass(bool bypass);
    virtual bool is_bypassed() const;
    
    // Serialization
    virtual DSPState save_state() const;
    virtual void load_state(const DSPState& state);
};
```

---

## 16. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-DSP-001 | SIMD Abstraction & Dispatch | 🔲 Pending |
| RFC-DSP-002 | Buffer Math & Core Primitives | 🔲 Pending |
| RFC-DSP-003 | Biquad Filter (RBJ cookbook) | 🔲 Pending |
| RFC-DSP-004 | State Variable Filter | 🔲 Pending |
| RFC-DSP-005 | Parametric EQ (8-band) | 🔲 Pending |
| RFC-DSP-006 | Linear Phase EQ | 🔲 Pending |
| RFC-DSP-007 | Compressor (Peak/RMS/SC) | 🔲 Pending |
| RFC-DSP-008 | Multiband Compressor | 🔲 Pending |
| RFC-DSP-009 | Brickwall Limiter (True Peak) | 🔲 Pending |
| RFC-DSP-010 | Delay (Mono/Stereo/PingPong) | 🔲 Pending |
| RFC-DSP-011 | Chorus, Flanger, Phaser | 🔲 Pending |
| RFC-DSP-012 | Algorithmic Reverb | 🔲 Pending |
| RFC-DSP-013 | Convolution Reverb | 🔲 Pending |
| RFC-DSP-014 | Waveshaper (Distortion Curves) | 🔲 Pending |
| RFC-DSP-015 | Bit Crusher & Downsampler | 🔲 Pending |
| RFC-DSP-016 | Stereo Tools (Width, M/S) | 🔲 Pending |
| RFC-DSP-017 | FFT & Spectral Processing | 🔲 Pending |
| RFC-DSP-018 | Pitch Detection (YIN/Cepstrum/HPS) | 🔲 Pending |
| RFC-DSP-019 | Transient Detection | 🔲 Pending |
| RFC-DSP-020 | Time Stretch & Pitch Shift | 🔲 Pending |
| RFC-DSP-021 | Tone Generator & Test Signals | 🔲 Pending |
| RFC-DSP-022 | Denormal Protection | 🔲 Pending |
| RFC-DSP-023 | Oversampling (for distortion) | 🔲 Pending |

---

## Appendix A: DSP Performance Benchmarks

| Operation | SSE2 | AVX2 | AVX512 | NEON |
|---|---|---|---|---|
| Buffer copy (1024 samples) | 0.8μs | 0.4μs | 0.2μs | 0.7μs |
| Buffer multiply gain | 0.7μs | 0.4μs | 0.2μs | 0.6μs |
| Biquad filter (per sample) | 8ns | 5ns | 3ns | 7ns |
| FFT 2048 | 12μs | 6μs | 3μs | 10μs |
| Compressor (per sample) | 15ns | 10ns | 5ns | 13ns |
| Reverb (per sample) | 40ns | 25ns | 12ns | 35ns |
| Waveshaper (per sample) | 5ns | 3ns | 2ns | 4ns |

## Appendix B: Latency Table

| Processor | Latency | Notes |
|---|---|---|
| Biquad filter | 0 | Minimum phase |
| Parametric EQ (8 bands) | 0 | Cascaded biquads |
| Linear Phase EQ | FFT/2 | FFT size dependant |
| Compressor | 0 | Lookahead adds latency |
| Limiter | 5-10 samples | Lookahead |
| Delay | delay_ms | Self-explanatory |
| Reverb (algorithmic) | pre_delay_ms | Typically 30-50ms |
| Convolution Reverb | FFT/2 | Partitioned convolution |
| Time Stretch | 512-2048 samples | Analysis window |
| FFT analysis | 0 | Parallel processing |

## Appendix C: Denormal Handling

All recursive DSP processors (filters, reverbs, delays) include denormal protection:

```cpp
// Denormal protection strategies:
// 1. FLUSH_TO_ZERO mode (MXCSR register on x86)
// 2. Small DC offset (1e-25) added to feedback paths
// 3. Denormal detection + flush in critical paths
//
// On ARM NEON: denormals are always flushed to zero (no penalty)
// On x86: must be explicitly enabled or managed
```
