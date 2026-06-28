#include "mixer/channel.h"

#include <algorithm>
#include <cmath>

namespace aria {

// ── Channel ─────────────────────────────────────────────────────

Channel::Channel(ChannelID id, const std::string& name, ChannelType type)
    : id_(id)
    , name_(name)
    , type_(type)
{
}

void Channel::set_volume(double db) {
    volume_db_.store(std::clamp(db, -120.0, 24.0));
}

void Channel::set_pan(double pan) {
    pan_.store(std::clamp(pan, -1.0, 1.0));
}

void Channel::set_width(double w) {
    width_.store(std::clamp(w, 0.0, 2.0));
}

double Channel::effective_volume() const {
    // Combine base volume + automation + VCA contribution
    double base  = volume_db_.load();
    double auto_ = automated_volume_db_.load();
    double vca   = vca_contribution_db_.load();
    return base + auto_ + vca;
}

double Channel::linear_volume() const {
    return db_to_linear(effective_volume());
}

double Channel::db_to_linear(double db) {
    return (db <= -120.0) ? 0.0 : std::pow(10.0, db / 20.0);
}

double Channel::linear_to_db(double linear) {
    return (linear <= 0.0) ? -120.0 : 20.0 * std::log10(linear);
}

// ── FX Chain ────────────────────────────────────────────────────

void Channel::add_fx(PluginID plugin, uint32_t position) {
    FXSlot slot;
    slot.plugin   = plugin;
    slot.bypassed = false;

    if (position >= fx_plugins_.size()) {
        fx_plugins_.push_back(std::move(slot));
    } else {
        fx_plugins_.insert(
            fx_plugins_.begin() + static_cast<ptrdiff_t>(position),
            std::move(slot));
    }
}

void Channel::remove_fx(uint32_t index) {
    if (index < fx_plugins_.size()) {
        fx_plugins_.erase(fx_plugins_.begin() + static_cast<ptrdiff_t>(index));
    }
}

void Channel::set_fx_bypass(uint32_t index, bool bypass) {
    if (index < fx_plugins_.size()) {
        fx_plugins_[index].bypassed = bypass;
    }
}

// ── Sends ───────────────────────────────────────────────────────

SendID Channel::add_send(ChannelID target, double level_db, bool pre_fader) {
    SendEntry entry;
    entry.id              = next_send_id_++;
    entry.slot.target     = target;
    entry.slot.level_db   = level_db;
    entry.slot.pre_fader  = pre_fader;
    entry.slot.pre_fx     = false;

    send_entries_.push_back(std::move(entry));
    return send_entries_.back().id;
}

void Channel::remove_send(SendID id) {
    auto it = std::find_if(send_entries_.begin(), send_entries_.end(),
        [id](const SendEntry& e) { return e.id == id; });
    if (it != send_entries_.end()) {
        send_entries_.erase(it);
    }
}

void Channel::set_send_level(SendID id, double db) {
    auto it = std::find_if(send_entries_.begin(), send_entries_.end(),
        [id](const SendEntry& e) { return e.id == id; });
    if (it != send_entries_.end()) {
        it->slot.level_db = std::clamp(db, -120.0, 24.0);
    }
}

std::vector<SendSlot> Channel::sends() const {
    std::vector<SendSlot> out;
    out.reserve(send_entries_.size());
    for (const auto& e : send_entries_) {
        out.push_back(e.slot);
    }
    return out;
}

} // namespace aria
