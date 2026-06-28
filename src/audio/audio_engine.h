#ifndef ARIA_AUDIO_ENGINE_H
#define ARIA_AUDIO_ENGINE_H

#include "audio_clock.h"
#include "audio_graph.h"
#include "audio_node.h"
#include "audio_node_types.h"
#include "buffer_pool.h"
#include "control_message_queue.h"
#include "diagnostics.h"
#include "metering/lufs_meter.h"
#include "metering/peak_meter.h"
#include "metering/rms_meter.h"
#include "metering/phase_meter.h"
#include "metering/spectrum_analyzer.h"
#include "multi_core_distributor.h"
#include "pdc_manager.h"
#include "simd_ops.h"
#include "src_converter.h"
#include "track_processor.h"
#include "transport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aria {

/// Audio Engine — real-time multi-track audio I/O and processing.
///
/// Manages the audio device lifecycle, buffer pool, processing graph,
/// transport state, audio clock, and real-time diagnostics.
///
/// Initialization flow:
///   1. detect_simd_level() — detect CPU SIMD capabilities
///   2. init() — set sample rate, buffer size, allocate pool
///   3. Build graph via input_node(), output_node(), graph()
///
/// Pipeline (per audio callback):
///   1. diagnostics.begin_callback()
///   2. Check transport state
///   3. process_control_messages() — execute queued control commands
///   4. Process audio graph via multi-core distributor
///   5. Update clock
///   6. Update meters
///   7. diagnostics.end_callback()
///
/// The transport and clock are accessible for control-thread operations
/// (play, stop, record, pause, seek) and are automatically updated on
/// every process() call.
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    /// Initialize the engine.
    /// @param sample_rate  Target sample rate (Hz).
    /// @param buffer_size  Number of frames per callback.
    /// @return true on success.
    bool init(uint32_t sample_rate, uint32_t buffer_size);

    /// Shut down and release all resources.
    void shutdown();

    bool is_initialized() const { return initialized_; }

    // ─── Accessors ─────────────────────────────────────────────

    uint32_t sample_rate() const { return sample_rate_; }
    uint32_t buffer_size() const { return buffer_size_; }

    /// The processing graph. Add nodes and connections here.
    AudioGraph& graph() { return graph_; }
    const AudioGraph& graph() const { return graph_; }

    /// The buffer pool for all audio processing.
    LockFreeBufferPool& pool() { return pool_; }
    const LockFreeBufferPool& pool() const { return pool_; }

    /// Detected SIMD level.
    SimdLevel simd_level() const { return simd_level_; }

    // ─── Transport ─────────────────────────────────────────────
    /// Access the transport (play, stop, pause, record, set_loop, seek).
    Transport& transport() { return transport_; }
    const Transport& transport() const { return transport_; }

    // ─── Audio Clock ───────────────────────────────────────────
    /// Access the audio clock (sample position, BPM, beat position).
    AudioClock& clock() { return clock_; }
    const AudioClock& clock() const { return clock_; }

    // ─── Control Messages ──────────────────────────────────────
    /// Queue a message for execution on the audio thread.
    /// Called from the control (UI) thread.
    /// @param msg  Function to execute on the audio thread.
    /// @return true if queued, false if queue full.
    bool push_control_message(ControlMessageQueue::Message msg) {
        return control_queue_.push(std::move(msg));
    }

    /// Access the control message queue.
    ControlMessageQueue& control_queue() { return control_queue_; }

    // ─── Tracks ────────────────────────────────────────────────

    /// Create a new track processor.
    /// @return Index of the new track.
    uint32_t add_track();

    /// Remove a track processor.
    /// @param index  Track index to remove.
    void remove_track(uint32_t index);

    /// Get a track processor by index.
    TrackProcessor* track(uint32_t index) const;

    /// Number of tracks.
    uint32_t track_count() const { return static_cast<uint32_t>(tracks_.size()); }

    // ─── Multi-core distribution ───────────────────────────────

    /// Access the multi-core distributor.
    MultiCoreDistributor& multicore() { return multicore_; }
    const MultiCoreDistributor& multicore() const { return multicore_; }

    /// Enable or disable multi-core distribution.
    void set_multicore_enabled(bool enabled) { multicore_.set_enabled(enabled); }
    bool multicore_enabled() const { return multicore_.is_enabled(); }

    // ─── Diagnostics ───────────────────────────────────────────

    /// Access the audio diagnostics.
    AudioDiagnostics& diagnostics() { return diagnostics_; }
    const AudioDiagnostics& diagnostics() const { return diagnostics_; }

    // ─── Master meter ──────────────────────────────────────────

    /// Master output peak level (linear).
    float master_peak() const { return master_peak_; }

    /// Master output RMS level (linear).
    float master_rms() const { return master_rms_; }

    // ─── Metering ───────────────────────────────────────────────

    /// Per-track peak meters.
    metering::PeakMeter& track_peak(uint32_t track) {
        return track_peak_meters_[track % kMaxTrackPeakMeters];
    }

    /// Master bus LUFS meter.
    metering::LUFSMeter& master_lufs() { return master_lufs_; }
    const metering::LUFSMeter& master_lufs() const { return master_lufs_; }

    /// Spectrum analyzer for master output.
    metering::SpectrumAnalyzer& spectrum() { return spectrum_; }
    const metering::SpectrumAnalyzer& spectrum() const { return spectrum_; }

    /// Phase correlation meter for master output.
    metering::PhaseMeter& phase_meter() { return phase_meter_; }
    const metering::PhaseMeter& phase_meter() const { return phase_meter_; }

    /// SRC converter for sample rate conversion.
    SRCConverter& src_converter() { return src_converter_; }
    const SRCConverter& src_converter() const { return src_converter_; }

    // ─── Engine process ────────────────────────────────────────

    /// Process one audio callback.
    /// Called from audio thread — must be real-time safe.
    /// @param frames          Number of frames in this block.
    /// @param sample_position Global sample position.
    void process(uint32_t frames, uint64_t sample_position);

private:
    /// Process pending control messages.
    void process_control_messages();

    /// Update master output metering.
    void update_meters();

    /// Process control messages that modify state.
    struct ControlMessage {
        enum class Type : uint8_t {
            AddTrack,
            RemoveTrack,
            SetGain,
            SetPan,
            SetMute,
            SetSolo,
            AddPlugin,
            RemovePlugin,
        };

        Type type;
        uint32_t track_index;
        float value;
        uint32_t extra;
    };

    bool initialized_ = false;
    uint32_t sample_rate_ = 48000;
    uint32_t buffer_size_ = 128;

    LockFreeBufferPool  pool_;
    AudioGraph          graph_;
    SimdLevel           simd_level_ = SimdLevel::Scalar;

    // Transport and clock
    Transport   transport_;
    AudioClock  clock_;

    // Track processing
    std::vector<std::unique_ptr<TrackProcessor>> tracks_;

    // Multi-core distribution
    MultiCoreDistributor multicore_;

    // Diagnostics
    AudioDiagnostics diagnostics_;

    // Control message queue (control thread → audio thread)
    ControlMessageQueue control_queue_;

    // Master meter readings
    float master_peak_ = 0.0f;
    float master_rms_ = 0.0f;

    // Meter accumulation buffer (pre-allocated)
    std::vector<float> meter_scratch_;

    // ─── Metering instances ─────────────────────────────────────
    static constexpr uint32_t kMaxTrackPeakMeters = 32;
    metering::PeakMeter     track_peak_meters_[kMaxTrackPeakMeters];
    metering::LUFSMeter     master_lufs_;
    metering::SpectrumAnalyzer spectrum_;
    metering::PhaseMeter    phase_meter_;
    metering::RMSMeter      master_rms_meter_;

    // ─── Sample Rate Converter ──────────────────────────────────
    SRCConverter src_converter_;
};

} // namespace aria

#endif // ARIA_AUDIO_ENGINE_H
