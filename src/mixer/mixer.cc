#include "mixer/mixer.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace aria {

// Utility: constexpr pi for Windows compatibility
#ifndef M_PI
static constexpr double kPi = 3.14159265358979323846;
#else
static constexpr double kPi = M_PI;
#endif

// ── Mixer ───────────────────────────────────────────────────────

Mixer::Mixer() = default;
Mixer::~Mixer() { shutdown(); }

bool Mixer::init(uint32_t sample_rate) {
    shutdown();

    sample_rate_ = sample_rate;
    next_channel_id_ = 1;
    next_bus_id_ = 1;

    // Create master channel
    master_id_ = create_channel("Master", ChannelType::Master);

    return true;
}

void Mixer::shutdown() {
    channels_.clear();
    buses_.clear();
    channel_to_bus_.clear();
    processing_order_.clear();
    master_id_ = kInvalidChannelID;
    order_dirty_ = true;
}

// ── Channel Management ──────────────────────────────────────────

ChannelID Mixer::create_channel(const std::string& name, ChannelType type) {
    if (channels_.size() >= config_.max_channels) {
        return kInvalidChannelID;
    }

    ChannelID id = next_channel_id_++;
    auto ch = std::make_unique<Channel>(id, name, type);
    channels_[id] = std::move(ch);
    order_dirty_ = true;
    return id;
}

void Mixer::destroy_channel(ChannelID id) {
    if (id == master_id_) return;  // cannot destroy master

    auto it = channels_.find(id);
    if (it == channels_.end()) return;

    // Remove from bus assignments
    auto bus_it = channel_to_bus_.find(id);
    if (bus_it != channel_to_bus_.end()) {
        BusID bus = bus_it->second;
        auto& members = buses_[bus].members;
        auto m = std::find(members.begin(), members.end(), id);
        if (m != members.end()) members.erase(m);
        channel_to_bus_.erase(bus_it);
    }

    channels_.erase(it);
    order_dirty_ = true;
}

Channel* Mixer::get_channel(ChannelID id) {
    auto it = channels_.find(id);
    return (it != channels_.end()) ? it->second.get() : nullptr;
}

const Channel* Mixer::get_channel(ChannelID id) const {
    auto it = channels_.find(id);
    return (it != channels_.end()) ? it->second.get() : nullptr;
}

std::vector<ChannelID> Mixer::all_channels() const {
    std::vector<ChannelID> ids;
    ids.reserve(channels_.size());
    for (const auto& [id, _] : channels_) {
        ids.push_back(id);
    }
    return ids;
}

Channel* Mixer::master_channel() {
    return get_channel(master_id_);
}

const Channel* Mixer::master_channel() const {
    return get_channel(master_id_);
}

// ── Buses ───────────────────────────────────────────────────────

BusID Mixer::create_bus(const std::string& name) {
    if (buses_.size() >= config_.max_buses) {
        return kInvalidBusID;
    }

    BusID id = next_bus_id_++;
    buses_[id] = BusInfo{name, {}};
    return id;
}

void Mixer::destroy_bus(BusID id) {
    auto it = buses_.find(id);
    if (it == buses_.end()) return;

    // Remove all channel assignments for this bus
    for (auto ch_id : it->second.members) {
        channel_to_bus_.erase(ch_id);
    }

    buses_.erase(it);
    order_dirty_ = true;
}

void Mixer::assign_channel_to_bus(ChannelID ch, BusID bus) {
    if (!channels_.count(ch)) return;
    if (!buses_.count(bus)) return;

    // Remove from previous bus if any
    remove_channel_from_bus(ch);

    // Add to new bus
    buses_[bus].members.push_back(ch);
    channel_to_bus_[ch] = bus;
    order_dirty_ = true;
}

void Mixer::remove_channel_from_bus(ChannelID ch) {
    auto it = channel_to_bus_.find(ch);
    if (it == channel_to_bus_.end()) return;

    BusID bus = it->second;
    auto& members = buses_[bus].members;
    auto m = std::find(members.begin(), members.end(), ch);
    if (m != members.end()) members.erase(m);
    channel_to_bus_.erase(it);
    order_dirty_ = true;
}

BusID Mixer::channel_bus(ChannelID ch) const {
    auto it = channel_to_bus_.find(ch);
    return (it != channel_to_bus_.end()) ? it->second : kInvalidBusID;
}

std::string Mixer::bus_name(BusID id) const {
    auto it = buses_.find(id);
    return (it != buses_.end()) ? it->second.name : "";
}

std::vector<BusID> Mixer::all_buses() const {
    std::vector<BusID> ids;
    ids.reserve(buses_.size());
    for (const auto& [id, _] : buses_) {
        ids.push_back(id);
    }
    return ids;
}

// ── Processing Order ────────────────────────────────────────────

void Mixer::rebuild_processing_order() {
    processing_order_.clear();

    // Collect all non-master channel IDs
    std::vector<ChannelID> audio_channels;
    std::vector<ChannelID> return_channels;
    std::vector<ChannelID> group_channels;
    std::vector<ChannelID> midi_channels;

    for (const auto& [id, ch] : channels_) {
        if (id == master_id_) continue;
        switch (ch->type()) {
            case ChannelType::Audio:
            case ChannelType::VCA:
                audio_channels.push_back(id);
                break;
            case ChannelType::Return:
                return_channels.push_back(id);
                break;
            case ChannelType::Group:
                group_channels.push_back(id);
                break;
            case ChannelType::MIDI:
                midi_channels.push_back(id);
                break;
            case ChannelType::Master:
                break; // handled above
        }
    }

    // Processing order: Audio → MIDI → Sends → Returns → Groups → Master
    for (auto id : audio_channels)   processing_order_.push_back(id);
    for (auto id : midi_channels)    processing_order_.push_back(id);
    for (auto id : return_channels)  processing_order_.push_back(id);
    for (auto id : group_channels)   processing_order_.push_back(id);

    // Master is always last
    if (master_id_ != kInvalidChannelID) {
        processing_order_.push_back(master_id_);
    }

    order_dirty_ = false;
}

// ── Pan Law ─────────────────────────────────────────────────────

void Mixer::apply_equal_power_pan(float in_l, float in_r, double pan,
                                   float* out_l, float* out_r) const {
    // Equal-power pan: -1 → full left, 0 → center, +1 → full right
    double angle = (pan + 1.0) * kPi / 4.0;  // -1 → 0, 0 → π/4, +1 → π/2
    *out_l = static_cast<float>(in_l * std::cos(angle));
    *out_r = static_cast<float>(in_r * std::sin(angle));
}

// ── Processing ──────────────────────────────────────────────────

void Mixer::process(AudioBuffer** inputs, uint32_t num_inputs,
                    AudioBuffer* master_output, uint32_t frames)
{
    if (!master_output || frames == 0) return;
    if (order_dirty_) rebuild_processing_order();

    // ── Determine solo state ──
    // If ANY channel is soloed, only soloed channels + return channels are audible
    bool any_solo = false;
    for (const auto& [id, ch] : channels_) {
        if (ch->is_soloed()) {
            any_solo = true;
            break;
        }
    }

    // ── Temporary mix accumulator (64-bit float) ──
    // We accumulate each channel's output into a per-bus or master mix buffer.
    // For simplicity: direct-to-master channels accumulate into master_mix,
    // bus-assigned channels accumulate into their bus's buffer (which later goes to master).

    // Allocate temporary double-precision mix buffers per bus + master
    // Use std::vector for dynamic storage
    constexpr uint32_t kMaxMixChannels = 2; // stereo for now
    std::vector<double> master_mix(static_cast<size_t>(frames) * kMaxMixChannels, 0.0);

    // Per-bus mix buffers
    struct BusMix {
        BusID bus_id;
        std::vector<double> buffer;
    };
    std::vector<BusMix> bus_mixes;
    bus_mixes.reserve(buses_.size());
    for (const auto& [bid, _] : buses_) {
        BusMix bm;
        bm.bus_id = bid;
        bm.buffer.resize(static_cast<size_t>(frames) * kMaxMixChannels, 0.0);
        bus_mixes.push_back(std::move(bm));
    }

    // ── Process each channel ──
    for (auto ch_id : processing_order_) {
        auto* ch = get_channel(ch_id);
        if (!ch) continue;

        // Skip muted channels
        if (ch->is_muted()) continue;

        // Solo logic: if any solo active, skip non-soloed non-return channels
        if (any_solo && !ch->is_soloed() && ch->type() != ChannelType::Return) continue;

        // MIDI and VCA channels produce no audio
        if (ch->type() == ChannelType::MIDI || ch->type() == ChannelType::VCA) continue;

        // Get input audio data
        // For Master and some channels, inputs may come from outside
        bool has_input = false;
        const float* in_l = nullptr;
        const float* in_r = nullptr;
        if (ch->type() == ChannelType::Master) {
            // Master channel input comes from the accumulated mix buffers
            continue; // handled after the loop
        } else if (ch->input_index() < num_inputs && inputs[ch->input_index()] != nullptr) {
            const auto* buf = inputs[ch->input_index()];
            if (buf->channels >= 1) in_l  = buf->channel(0);
            if (buf->channels >= 2) in_r  = buf->channel(0); // mono input, will be copied
            if (buf->channels >= 2 && buf->data[1]) in_r = buf->channel(1);
            has_input = (in_l != nullptr);
        }

        if (!has_input) continue;

        // Process channel audio
        double linear_gain = ch->linear_volume();
        double pan         = ch->pan();
        bool   invert      = ch->phase_inverted();
        double width       = ch->width();

        // Determine destination: bus or master
        BusID assigned_bus = channel_bus(ch_id);
        double* mix_buffer = nullptr;
        if (assigned_bus != kInvalidBusID) {
            auto bit = std::find_if(bus_mixes.begin(), bus_mixes.end(),
                [assigned_bus](const BusMix& bm) { return bm.bus_id == assigned_bus; });
            if (bit != bus_mixes.end()) {
                mix_buffer = bit->buffer.data();
            }
        }
        if (!mix_buffer) {
            mix_buffer = master_mix.data();
        }

        // Accumulate into mix buffer
        const uint32_t input_frames = inputs[ch->input_index()]->frames;
        const uint32_t limit = (frames < input_frames) ? frames : input_frames;
        for (uint32_t i = 0; i < limit; ++i) {
            float s_l = in_l[i];
            float s_r = (in_r) ? in_r[i] : s_l;

            // Apply width (simplified mid/side)
            if (width != 1.0) {
                float mid = (s_l + s_r) * 0.5f;
                float side = (s_r - s_l) * 0.5f;
                s_l = static_cast<float>(mid - side * width);
                s_r = static_cast<float>(mid + side * width);
            }

            // Apply pan
            float panned_l, panned_r;
            apply_equal_power_pan(s_l, s_r, pan, &panned_l, &panned_r);

            // Apply phase invert
            if (invert) {
                panned_l = -panned_l;
                panned_r = -panned_r;
            }

            // Apply gain and accumulate into mix buffer (64-bit)
            mix_buffer[i * 2 + 0] += static_cast<double>(panned_l) * linear_gain;
            mix_buffer[i * 2 + 1] += static_cast<double>(panned_r) * linear_gain;
        }
    }

    // ── Sum buses into master ──
    for (const auto& bm : bus_mixes) {
        for (uint32_t i = 0; i < frames; ++i) {
            master_mix[i * 2 + 0] += bm.buffer[i * 2 + 0];
            master_mix[i * 2 + 1] += bm.buffer[i * 2 + 1];
        }
    }

    // ── Master output ──
    auto* master_ch = master_channel();
    double master_gain = master_ch ? master_ch->linear_volume() : 1.0;
    bool master_invert = master_ch ? master_ch->phase_inverted() : false;

    uint32_t out_ch = std::min<uint32_t>(master_output->channels, kMaxMixChannels);
    uint32_t out_frames = std::min(frames, master_output->frames);

    for (uint32_t i = 0; i < out_frames; ++i) {
        float s_l = static_cast<float>(master_mix[i * 2 + 0] * master_gain);
        float s_r = static_cast<float>(master_mix[i * 2 + 1] * master_gain);

        if (master_invert) {
            s_l = -s_l;
            s_r = -s_r;
        }

        if (out_ch >= 1 && master_output->data[0]) {
            master_output->data[0][i] = s_l;
        }
        if (out_ch >= 2 && master_output->data[1]) {
            master_output->data[1][i] = s_r;
        }
    }

    // ── Update channel metering ──
    // For now, just a simple peak update on each channel's input
    for (const auto& [id, ch] : channels_) {
        if (ch->input_index() < num_inputs && inputs[ch->input_index()] != nullptr) {
            const auto* buf = inputs[ch->input_index()];
            float peak_l = 0.0f, peak_r = 0.0f;
            float rms_l = 0.0f, rms_r = 0.0f;
            uint32_t n = std::min(frames, buf->frames);

            for (uint32_t i = 0; i < n; ++i) {
                float s_l = buf->data[0] ? buf->data[0][i] : 0.0f;
                float s_r = (buf->channels >= 2 && buf->data[1]) ? buf->data[1][i] : s_l;
                peak_l = std::max(peak_l, std::abs(s_l));
                peak_r = std::max(peak_r, std::abs(s_r));
                rms_l += s_l * s_l;
                rms_r += s_r * s_r;
            }

            if (n > 0) {
                rms_l = std::sqrt(rms_l / static_cast<float>(n));
                rms_r = std::sqrt(rms_r / static_cast<float>(n));
            }

            // Convert to dBFS
            auto to_dbfs = [](float linear) -> float {
                return (linear <= 0.0f) ? -60.0f : 20.0f * std::log10(linear);
            };

            ch->set_metering(
                to_dbfs(peak_l), to_dbfs(peak_r),
                to_dbfs(rms_l), to_dbfs(rms_r));
        }
    }
}

} // namespace aria
