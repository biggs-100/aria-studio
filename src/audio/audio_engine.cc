#include "audio_engine.h"

#include "export/file_writer.h"
#include "export/offline_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace aria {

AudioEngine::AudioEngine()
    : master_rms_meter_()
    , spectrum_()
    , phase_meter_()
    , src_converter_()
{
    // Detect SIMD capabilities on construction
    simd_level_ = detect_simd_level();

    // Configure SRC converter with default rates
    src_converter_.set_input_rate(44100);
    src_converter_.set_output_rate(48000);
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init(uint32_t sample_rate, uint32_t buffer_size) {
    if (initialized_) return false;

    sample_rate_ = sample_rate;
    buffer_size_ = buffer_size;

    // Detect SIMD (in case constructor didn't run, e.g., after move)
    if (simd_level_ == SimdLevel::Scalar) {
        simd_level_ = detect_simd_level();
    }

    // Initialize buffer pool
    // Pool capacity: cover worst-case graph with all nodes producing output
    // + some headroom. Default 1024 is generous.
    const uint32_t pool_size = 1024;
    if (!pool_.init(buffer_size, kMaxChannels, pool_size)) {
        return false;
    }

    // Wire the pool into the graph
    graph_.set_pool(&pool_);

    // Initialize clock with sample rate
    clock_.set_sample_rate(static_cast<double>(sample_rate));

    // Initialize diagnostics with buffer duration
    uint64_t buffer_us = static_cast<uint64_t>(buffer_size) * 1'000'000ULL
                       / static_cast<uint64_t>(sample_rate);
    diagnostics_.set_buffer_duration_us(buffer_us);

    // Initialize multi-core distributor
    multicore_.init(0);  // auto-detect thread count

    // Configure metering with sample rate
    master_rms_meter_.set_window_ms(300.0);
    master_lufs_.reset();
    spectrum_.set_fft_size(2048);
    spectrum_.set_window_type(metering::SpectrumAnalyzer::WindowType::Hanning);
    phase_meter_.reset();

    // Configure SRC converter
    src_converter_.set_input_rate(sample_rate);
    src_converter_.set_output_rate(sample_rate);
    src_converter_.reset();

    // Pre-allocate meter scratch buffer
    meter_scratch_.resize(buffer_size * kMaxChannels);

    initialized_ = true;
    return true;
}

void AudioEngine::shutdown() {
    if (!initialized_) return;

    // Shut down multicore first (joins worker threads)
    multicore_.shutdown();

    // Reset meters
    master_lufs_.reset();
    spectrum_.reset();
    phase_meter_.reset();
    src_converter_.reset();

    // Clear tracks
    tracks_.clear();

    initialized_ = false;
}

// ─── Track management ─────────────────────────────────────────

uint32_t AudioEngine::add_track() {
    auto track = std::make_unique<TrackProcessor>();
    track->configure(2, static_cast<double>(sample_rate_));
    uint32_t index = static_cast<uint32_t>(tracks_.size());
    tracks_.push_back(std::move(track));

    // Rebalance multi-core distribution
    std::vector<TrackProcessor*> track_ptrs;
    track_ptrs.reserve(tracks_.size());
    for (auto& tp : tracks_) {
        track_ptrs.push_back(tp.get());
    }
    multicore_.rebalance(track_ptrs);

    return index;
}

void AudioEngine::remove_track(uint32_t index) {
    if (index >= tracks_.size()) return;
    tracks_.erase(tracks_.begin() + index);

    // Rebalance multi-core distribution
    std::vector<TrackProcessor*> track_ptrs;
    track_ptrs.reserve(tracks_.size());
    for (auto& tp : tracks_) {
        track_ptrs.push_back(tp.get());
    }
    multicore_.rebalance(track_ptrs);
}

TrackProcessor* AudioEngine::track(uint32_t index) const {
    if (index >= tracks_.size()) return nullptr;
    return tracks_[index].get();
}

// ─── Control message processing ───────────────────────────────

void AudioEngine::process_control_messages() {
    control_queue_.process_all();
}

// ─── Metering ─────────────────────────────────────────────────

void AudioEngine::update_meters() {
    float peak = 0.0f;
    float rms = 0.0f;

    if (tracks_.empty()) {
        master_peak_ = 0.0f;
        master_rms_ = 0.0f;
        return;
    }

    // Collect peak and RMS from all active tracks
    // In a full implementation, tracks share a master bus buffer.
    // For now, use the track processors' metering if available.
    uint32_t track_samples = 0;
    double sum_sq = 0.0;

    for (auto& tp : tracks_) {
        if (!tp || tp->is_muted()) continue;

        // Track processors have their own peak/RMS estimates
        // For now, we use the meter scratch buffer as a placeholder.
        // Real implementation would read from the master bus AudioBuffer.
        //
        // Update per-track peak meters (placeholder — no actual sample
        // data available until master bus routing is implemented).
        // track_peak_meters_[index].process(...)
    }

    // Use peak meter and RMS meter instances
    // (Readings are placeholder until master bus is wired)
    master_peak_ = peak;
    master_rms_ = static_cast<float>(std::sqrt(
        track_samples > 0 ? sum_sq / static_cast<double>(track_samples) : 0.0));
}

// ─── Main process callback ────────────────────────────────────

void AudioEngine::process(uint32_t frames, uint64_t /*sample_position*/) {
    if (!initialized_) return;

    // 1. Start diagnostic timing
    diagnostics_.begin_callback();

    // 2. Process control messages (lock-free queue)
    process_control_messages();

    // 3. Check transport state
    auto state = transport_.state();

    if (state == TransportState::Stopped) {
        // Output silence, but process the graph for any offline tasks
        graph_.process(frames, clock_.sample_position());

        // Update clock (even when stopped, for sample-accurate position)
        clock_.process(frames);

        diagnostics_.end_callback();
        return;
    }

    // 4. Update transport (advances clock with loop enforcement)
    transport_.process(frames, clock_);

    // 5. Process the audio graph (I/O nodes, mixer, etc.)
    graph_.process(frames, clock_.sample_position());

    // 5b. Feed spectrum analyzer with master output (placeholder —
    //     would read from master bus in full implementation)
    // spectrum_.process(master_bus_buffer, frames);

    // 6. Process all tracks using multi-core distribution
    if (!tracks_.empty()) {
        // Build track processor vector for distribution
        std::vector<TrackProcessor*> track_ptrs;
        track_ptrs.reserve(tracks_.size());
        for (auto& tp : tracks_) {
            track_ptrs.push_back(tp.get());
        }

        // Phase 1 (serial): graph processing already done above
        // Phase 2 (parallel): distribute tracks across cores
        // Phase 3 (serial): meter update
        multicore_.distribute(
            track_ptrs,
            frames,
            clock_.sample_position(),
            nullptr,   // phase1 already done
            [this]() { update_meters(); }
        );
    } else {
        // No tracks — just update meters
        update_meters();
    }

    // 7. Update clock (advance sample position)
    clock_.process(frames);

    // 8. End diagnostic timing
    diagnostics_.end_callback();
}

} // namespace aria
