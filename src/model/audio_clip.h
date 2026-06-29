#ifndef ARIA_MODEL_AUDIO_CLIP_H
#define ARIA_MODEL_AUDIO_CLIP_H

#include "model/clip.h"

#include <cstdint>
#include <string>
#include <vector>

namespace aria {

// ─── WaveformCache ─────────────────────────────────────────────

/// Pre-computed peak/minima pairs for waveform rendering at a given
/// level-of-detail (LOD). Each LOD level stores one peak+valley pair
/// per sample group (e.g., one pair per 441 source samples at 1/100th
/// zoom).
struct WaveformLOD {
    std::vector<float> peaks;
    std::vector<float> valleys;
};

/// Container for multi-resolution waveform data.
class WaveformCache {
public:
    WaveformCache() = default;

    /// Retrieve or lazily generate LOD data for the given resolution
    /// (samples-per-point). Returns a reference to the cached LOD.
    const WaveformLOD& get_lod(uint32_t samples_per_point,
                                uint64_t total_samples);

    /// Direct access to cached LODs (for inspection).
    const std::vector<WaveformLOD>& lods() const { return lods_; }

    /// Clear all cached LOD data.
    void clear() { lods_.clear(); lods_.shrink_to_fit(); }

private:
    std::vector<WaveformLOD> lods_;
};

// ─── GainPoint ─────────────────────────────────────────────────

/// A single point in an AudioClip's gain envelope.
/// Position is in PPQN, gain is normalized [0.0, 1.0].
struct GainPoint {
    uint64_t ppqn   = 0;
    double   gain   = 1.0;
};

// ─── AudioClip ─────────────────────────────────────────────────

/// A clip that references an audio file with waveform caching and
/// clip-local gain envelope.
class AudioClip : public Clip {
public:
    AudioClip() = default;

    // ─── File reference ───────────────────────────────────────
    void set_file_path(const std::string& path) { file_path_ = path; }
    const std::string& file_path() const { return file_path_; }

    void set_file_hash(uint64_t hash) { file_hash_ = hash; }
    uint64_t file_hash() const { return file_hash_; }

    // ─── Sample data ──────────────────────────────────────────
    void    set_total_samples(uint64_t n) { total_samples_ = n; }
    uint64_t total_samples() const { return total_samples_; }

    void    set_sample_offset(uint64_t n) { sample_offset_ = n; }
    uint64_t sample_offset() const { return sample_offset_; }

    /// Number of samples actually played (total - offset, clamped).
    uint64_t playback_samples() const;

    // ─── Waveform cache ───────────────────────────────────────
    /// Get or generate waveform LOD data at given resolution.
    const WaveformLOD& get_waveform(uint32_t samples_per_point);

    /// Access the internal cache (for inspection / serialization).
    const WaveformCache& waveform_cache() const { return waveform_cache_; }

    // ─── Gain envelope ────────────────────────────────────────
    void add_gain_point(const GainPoint& pt) { gain_envelope_.push_back(pt); }
    const std::vector<GainPoint>& gain_envelope() const { return gain_envelope_; }

    /// Evaluate the gain envelope at a given PPQN position.
    /// Falls back to 1.0 (unity) when envelope is empty.
    double gain_at_ppqn(uint64_t ppqn) const;

private:
    std::string  file_path_;
    uint64_t     file_hash_     = 0;
    uint64_t     total_samples_ = 0;
    uint64_t     sample_offset_ = 0;

    WaveformCache            waveform_cache_;
    std::vector<GainPoint>   gain_envelope_;
};

} // namespace aria

#endif // ARIA_MODEL_AUDIO_CLIP_H
