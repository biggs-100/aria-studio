#ifndef ARIA_TRACK_PROCESSOR_H
#define ARIA_TRACK_PROCESSOR_H

#include "audio_buffer.h"
#include "audio_node.h"
#include "audio/wsola.h"
#include "pdc_manager.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace aria {

// ─── Forward declarations ─────────────────────────────────────
class AudioGraph;

// ─── Clip playback slot ──────────────────────────────────────

/// Describes a clip's audio contribution for one process block.
struct ClipPlaybackSlot {
    uint64_t start_ppqn = 0;      ///< Clip start in PPQN
    uint64_t length_ppqn = 960;   ///< Clip duration in PPQN
    const float* source_data = nullptr; ///< Audio sample data (null = silent)
    uint32_t source_frames = 0;    ///< Total frames available in source_data
    uint32_t source_channels = 1;  ///< Channels in source_data
    double   gain = 1.0;           ///< Clip gain multiplier
    bool     is_looping = false;   ///< Whether the clip loops
};

/// Per-track audio processing.
///
/// Handles the real-time processing pipeline for a single track:
///   1. Read clips from the (future) model
///   2. Apply clip gain / automation
///   3. Apply time stretch (WSOLA or alternative)
///   4. Apply crossfader gain
///   5. Route through the track's plugin chain
///   6. Send to track output (bus or master)
///
/// All methods are real-time safe — no allocations, no locks.
class TrackProcessor {
public:
    TrackProcessor();
    ~TrackProcessor() = default;

    TrackProcessor(const TrackProcessor&) = delete;
    TrackProcessor& operator=(const TrackProcessor&) = delete;

    TrackProcessor(TrackProcessor&&) = default;
    TrackProcessor& operator=(TrackProcessor&&) = default;

    /// Configure the track.
    /// @param num_channels  Number of audio channels (1 mono, 2 stereo, etc.)
    /// @param sample_rate   Sample rate in Hz.
    void configure(uint32_t num_channels, double sample_rate);

    /// Process one audio block for this track.
    /// @param frames           Number of frames to process.
    /// @param sample_position  Global sample position for this block.
    /// @param input            Input buffer from graph (mix of clips + input bus).
    /// @param output           Output buffer to accumulate results into.
    void process(uint32_t frames, uint64_t sample_position,
                 AudioBuffer* input, AudioBuffer* output);

    /// True when the track has active audio data (clips or input).
    bool is_active() const { return active_.load(std::memory_order_relaxed); }

    /// Set track activity (called from control thread when clips change).
    void set_active(bool active) {
        active_.store(active, std::memory_order_release);
    }

    /// Track-level gain (linear amplitude).
    void set_gain(float gain) { gain_ = gain; }
    float gain() const { return gain_; }

    /// Track pan (-1.0 = full left, 0.0 = center, 1.0 = full right).
    void set_pan(float pan) { pan_ = pan; }
    float pan() const { return pan_; }

    /// Whether the track is muted.
    void set_muted(bool muted) { muted_.store(muted, std::memory_order_release); }
    bool is_muted() const { return muted_.load(std::memory_order_relaxed); }

    /// Whether the track is soloed.
    void set_soloed(bool soloed) { soloed_.store(soloed, std::memory_order_release); }
    bool is_soloed() const { return soloed_.load(std::memory_order_relaxed); }

    /// Number of channels.
    uint32_t channels() const { return num_channels_; }

    // ─── Plugin chain ──────────────────────────────────────────

    /// Add a plugin (AudioNode) to this track's chain.
    /// The plugin is owned by the track after this call.
    void add_plugin(std::unique_ptr<AudioNode> plugin);

    /// Remove the plugin at the given index.
    /// Returns ownership of the removed plugin (or nullptr).
    std::unique_ptr<AudioNode> remove_plugin(uint32_t index);

    /// Number of plugins in the chain.
    uint32_t plugin_count() const { return static_cast<uint32_t>(plugins_.size()); }

    /// Get a plugin at index (for control thread access).
    AudioNode* plugin(uint32_t index) const;

    /// Get the total latency of all plugins in the chain.
    uint32_t chain_latency() const;

    /// Access the PDC manager for this track.
    PDCManager& pdc() { return pdc_; }
    const PDCManager& pdc() const { return pdc_; }

    // ─── Clip playback ──────────────────────────────────────────

    /// Set the active clip slots for the current process block.
    /// Called from the control thread before process().
    void set_clip_slots(const std::vector<ClipPlaybackSlot>& slots) {
        clip_slots_ = slots;
        active_.store(!slots.empty(), std::memory_order_release);
    }

    /// Get the current clip slots.
    const std::vector<ClipPlaybackSlot>& clip_slots() const { return clip_slots_; }

    // ─── Crossfader gain ────────────────────────────────────────

    /// Set per-track crossfader gain from the Session crossfader.
    /// Range: 0.0 (silent) to 1.0 (unity).
    void set_crossfader_gain(float gain) { crossfader_gain_ = gain; }
    float crossfader_gain() const { return crossfader_gain_; }

    // ─── Time stretch ───────────────────────────────────────────

    /// Set the time-stretch algorithm for this track.
    /// Default: none (pass-through). Can be WSOLAStretch or a stub.
    void set_time_stretch_algorithm(std::unique_ptr<TimeStretchAlgorithm> algo) {
        stretch_ = std::move(algo);
    }

    /// Access the current stretch algorithm (may be null).
    TimeStretchAlgorithm* stretch_algorithm() const { return stretch_.get(); }

private:
    /// Apply time-stretch to the output buffer.
    void apply_time_stretch(AudioBuffer* buffer, uint32_t frames);

    /// Apply clip gain, crossfader gain, and panning to the buffer.
    void apply_clip_gain(AudioBuffer* buffer, uint32_t frames);

    uint32_t num_channels_ = 2;
    double sample_rate_ = 48000.0;

    // Track state
    float gain_ = 1.0f;
    float pan_ = 0.0f;
    float crossfader_gain_ = 1.0f;
    std::atomic<bool> active_{false};
    std::atomic<bool> muted_{false};
    std::atomic<bool> soloed_{false};

    // Plugin chain (owned)
    std::vector<std::unique_ptr<AudioNode>> plugins_;

    // Plugin delay compensation for this track
    PDCManager pdc_;

    // Per-block scratch buffer for plugin chain intermediate results.
    std::vector<float> scratch_;
    uint32_t scratch_capacity_ = 0;

    // Clip playback slots (set from control thread)
    std::vector<ClipPlaybackSlot> clip_slots_;

    // Time stretch algorithm (optional, default = null = pass-through)
    std::unique_ptr<TimeStretchAlgorithm> stretch_;
};

} // namespace aria

#endif // ARIA_TRACK_PROCESSOR_H
