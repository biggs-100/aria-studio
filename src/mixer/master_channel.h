#ifndef ARIA_MIXER_MASTER_CHANNEL_H
#define ARIA_MIXER_MASTER_CHANNEL_H

#include <cstdint>
#include <vector>
#include <cmath>
#include <random>

#include "mixer/channel.h"
#include "audio/audio_buffer.h"

namespace aria {

/// Master output processor — final gain stage, brickwall limiter,
/// dither, and master FX insert chain.
class MasterChannel {
public:
    MasterChannel();
    ~MasterChannel() = default;

    // ── Volume ──────────────────────────────────────────────────
    void   set_volume(double db);
    double volume() const { return volume_db_; }

    // ── Limiter ─────────────────────────────────────────────────
    struct LimiterConfig {
        bool    enabled      = true;
        double  threshold_db = -1.0;   // -0.1 to -6 dB
        double  ceiling_db   = -0.3;
        double  release_ms   = 100.0;  // release time in ms
    };

    void         set_limiter(const LimiterConfig& config);
    LimiterConfig limiter() const { return limiter_config_; }

    // ── Dither ──────────────────────────────────────────────────
    enum DitherType { None, Triangular, Shaped };

    void           set_dither(DitherType type, uint16_t bit_depth = 24);
    DitherType     dither() const { return dither_; }
    uint16_t       bit_depth() const { return bit_depth_; }

    // ── Master FX Chain ─────────────────────────────────────────
    void add_fx(PluginID plugin, uint32_t position);
    uint32_t fx_count() const { return static_cast<uint32_t>(fx_chain_.size()); }

    // ── Processing ──────────────────────────────────────────────
    /// Process the master bus through the full chain:
    ///   volume → master FX → limiter → dither → output
    void process(const AudioBuffer* input, AudioBuffer* output, uint32_t frames);

    // ── Sample rate ─────────────────────────────────────────────
    void set_sample_rate(double sample_rate) { sample_rate_ = sample_rate; }
    double sample_rate() const { return sample_rate_; }

    // ── Limiter metrics (for metering) ──────────────────────────
    double gain_reduction_db() const { return last_gain_reduction_db_; }

    // ── Brickwall Limiter (exposed for testing) ─────────────────
    struct BrickwallLimiter {
        double threshold_db = -1.0;
        double release_ms   = 100.0;
        double envelope     = 0.0;   // current envelope follower value
        double gain_reduction = 1.0; // current gain reduction factor

        /// Process a stereo buffer through the limiter.
        /// Mutates samples in-place.
        void process(float* samples, uint32_t frames, uint32_t channels, double sample_rate);

        /// Reset internal state.
        void reset();
    };

private:
    double volume_db_ = 0.0;
    LimiterConfig limiter_config_;
    BrickwallLimiter limiter_;
    DitherType dither_ = None;
    uint16_t bit_depth_ = 24;
    double sample_rate_ = 48000.0;
    double last_gain_reduction_db_ = 0.0;

    // Master FX insert chain (PluginID only, actual processing
    // is delegated to the plugin host).
    std::vector<PluginID> fx_chain_;

    // Dither RNG
    std::mt19937 rng_{42};

    // Apply triangular dither to a sample buffer in-place.
    void apply_triangular_dither(float* samples, uint32_t frames, uint32_t channels);
    // Apply shaped-noise dither (first-order highpass noise shaping).
    void apply_shaped_dither(float* samples, uint32_t frames, uint32_t channels);
};

} // namespace aria

#endif // ARIA_MIXER_MASTER_CHANNEL_H
