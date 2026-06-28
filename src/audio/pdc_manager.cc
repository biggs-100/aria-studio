#include "pdc_manager.h"
#include "simd_ops.h"

#include <algorithm>
#include <cstring>
#include <numeric>

namespace aria {

// ─── Move operations ──────────────────────────────────────────

PDCManager::PDCManager(PDCManager&& other) noexcept
    : max_delay_(other.max_delay_.load())
    , buffer_capacity_(other.buffer_capacity_)
{
    delay_buffers_ = std::move(other.delay_buffers_);
    other.max_delay_.store(0, std::memory_order_relaxed);
    other.buffer_capacity_ = 0;
}

PDCManager& PDCManager::operator=(PDCManager&& other) noexcept {
    if (this != &other) {
        delay_buffers_ = std::move(other.delay_buffers_);
        max_delay_.store(other.max_delay_.load(), std::memory_order_relaxed);
        buffer_capacity_ = other.buffer_capacity_;
        other.max_delay_.store(0, std::memory_order_relaxed);
        other.buffer_capacity_ = 0;
    }
    return *this;
}

// ─── DelayBuffer implementation ───────────────────────────────

void PDCManager::DelayBuffer::init(uint32_t capacity, uint32_t ch) {
    size = capacity;
    channels = ch;
    data.resize(size * channels);
    write_pos = 0;
    clear();
}

void PDCManager::DelayBuffer::clear() {
    std::memset(data.data(), 0, data.size() * sizeof(float));
}

void PDCManager::DelayBuffer::read(float* output, uint32_t frames) {
    if (size == 0 || frames == 0) return;

    uint32_t read_pos = (write_pos + size - frames) % size;
    for (uint32_t i = 0; i < frames; ++i) {
        output[i] = data[(read_pos + i) % size];
    }
}

void PDCManager::DelayBuffer::write(const float* input, uint32_t frames) {
    if (size == 0 || frames == 0) return;

    for (uint32_t i = 0; i < frames; ++i) {
        data[write_pos] = input[i];
        write_pos = (write_pos + 1) % size;
    }
}

// ─── PDCManager implementation ────────────────────────────────

void PDCManager::recalculate(const std::vector<AudioNode*>& chain) {
    uint32_t total_delay = chain_delay(chain);

    // Update max_delay to the chain's total latency
    uint32_t current_max = max_delay_.load(std::memory_order_relaxed);
    if (total_delay > current_max) {
        max_delay_.store(total_delay, std::memory_order_release);
    }

    // Ensure delay buffer capacity is sufficient
    if (total_delay > buffer_capacity_) {
        buffer_capacity_ = total_delay + 64;  // small headroom
    }
}

uint32_t PDCManager::chain_delay(const std::vector<AudioNode*>& chain) const {
    return std::accumulate(chain.begin(), chain.end(), uint32_t{0},
        [](uint32_t sum, const AudioNode* node) {
            return sum + (node ? node->latency() : 0);
        });
}

void PDCManager::apply_compensation(AudioBuffer& buffer, uint32_t delay_samples,
                                     uint64_t chain_id) {
    if (delay_samples == 0) return;
    if (buffer.frames == 0 || buffer.channels == 0) return;

    // Get or create delay buffer for this chain
    auto it = delay_buffers_.find(chain_id);
    if (it == delay_buffers_.end()) {
        // Allocate new delay buffer: need at least delay_samples + buffer.frames
        // to handle one callback's worth of data plus the delay
        DelayBuffer db;
        db.init(delay_samples + buffer.frames, buffer.channels);
        auto result = delay_buffers_.emplace(chain_id, std::move(db));
        it = result.first;
    }

    DelayBuffer& db = it->second;

    // Ensure the delay buffer is large enough (grow if needed)
    uint32_t needed = delay_samples + buffer.frames;
    if (db.size < needed) {
        // Grow buffer — NOT real-time safe, but only hit during PDC reconfig
        DelayBuffer new_db;
        new_db.init(needed, buffer.channels);
        // Copy old data preserving relative position
        if (db.size > 0 && db.channels > 0) {
            uint32_t copy_ch = std::min(db.channels, new_db.channels);
            for (uint32_t c = 0; c < copy_ch; ++c) {
                uint32_t old_offset = c * db.size;
                uint32_t new_offset = c * new_db.size;
                // Copy oldest samples first, write_pos wraps around
                for (uint32_t i = 0; i < db.size; ++i) {
                    uint32_t src_idx = (db.write_pos + i) % db.size;
                    new_db.data[new_offset + i] = db.data[old_offset + src_idx];
                }
            }
            new_db.write_pos = db.size % new_db.size;
        }
        delay_buffers_[chain_id] = std::move(new_db);
        db = delay_buffers_[chain_id];
    }

    // Process each channel sample-by-sample through the delay line
    for (uint32_t c = 0; c < buffer.channels; ++c) {
        float* ch_data = buffer.data[c];
        if (!ch_data) continue;

        const uint32_t channel_offset = c * db.size;

        // For each sample in this block:
        // 1. Read delayed sample from (write_pos - delay_samples)
        // 2. Write current input sample to write_pos
        // 3. Advance write_pos
        for (uint32_t i = 0; i < buffer.frames; ++i) {
            // Calculate read position: delay_samples behind write_pos
            uint32_t read_pos = (db.write_pos + db.size - delay_samples) % db.size;

            // Read delayed sample
            float delayed = db.data[channel_offset + read_pos];

            // Write current input sample
            db.data[channel_offset + db.write_pos] = ch_data[i];

            // Store delayed sample as output
            ch_data[i] = delayed;

            // Advance write position
            db.write_pos = (db.write_pos + 1) % db.size;
        }
    }
}

void PDCManager::reset() {
    for (auto& [id, buf] : delay_buffers_) {
        buf.clear();
        buf.write_pos = 0;
    }
    max_delay_.store(0, std::memory_order_release);
    buffer_capacity_ = 0;
    delay_buffers_.clear();
}

} // namespace aria
