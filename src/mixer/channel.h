#ifndef ARIA_MIXER_CHANNEL_H
#define ARIA_MIXER_CHANNEL_H

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <algorithm>
#include <cmath>

#include "mixer/builtin_eq.h"

namespace aria {

// ── ID Types ────────────────────────────────────────────────────
using ChannelID = uint32_t;
using BusID     = uint32_t;
using SendID    = uint32_t;
using TrackID   = uint64_t;   // matches model/track.h
using PluginID  = uint64_t;

static constexpr ChannelID kInvalidChannelID = UINT32_MAX;
static constexpr BusID     kInvalidBusID     = UINT32_MAX;
static constexpr SendID    kInvalidSendID    = UINT32_MAX;
static constexpr TrackID   kInvalidTrackID   = UINT64_MAX;

// ── Channel Type ────────────────────────────────────────────────
enum class ChannelType {
    Audio,
    MIDI,
    Group,
    Return,
    VCA,
    Master
};

// ── Send Slot ───────────────────────────────────────────────────
struct SendSlot {
    ChannelID target = 0;
    double level_db = -60.0;
    bool pre_fader = true;
    bool pre_fx = false;
};

// ── Channel Strip ───────────────────────────────────────────────
class Channel {
public:
    Channel(ChannelID id, const std::string& name, ChannelType type);
    virtual ~Channel() = default;

    // Identity
    ChannelID   id() const             { return id_; }
    std::string name() const           { return name_; }
    void        set_name(const std::string& name) { name_ = name; }
    ChannelType type() const           { return type_; }

    // Level controls
    void   set_volume(double db);        // -∞ to +24 dB
    double volume() const                { return volume_db_.load(); }
    void   set_pan(double pan);          // -1.0 (L) to +1.0 (R)
    double pan() const                   { return pan_.load(); }
    void   set_mute(bool mute)           { muted_.store(mute); }
    bool   is_muted() const              { return muted_.load(); }
    void   set_solo(bool solo)           { soloed_.store(solo); }
    bool   is_soloed() const             { return soloed_.load(); }
    void   set_phase_invert(bool inv)    { phase_invert_.store(inv); }
    bool   phase_inverted() const        { return phase_invert_.load(); }
    void   set_width(double w);          // 0.0 (mono) to 2.0 (wide)
    double width() const                 { return width_.load(); }

    // Automation override
    void   set_automated_volume(double db) { automated_volume_db_.store(db); }
    double effective_volume() const;

    // Groups / VCA
    void     set_group(BusID bus)   { group_bus_ = bus; }
    BusID    group() const          { return group_bus_; }
    void     set_vca(TrackID vca)   { vca_track_ = vca; }
    TrackID  vca() const            { return vca_track_; }

    // FX
    void     add_fx(PluginID plugin, uint32_t position);
    void     remove_fx(uint32_t index);
    void     set_fx_bypass(uint32_t index, bool bypass);
    uint32_t fx_count() const       { return static_cast<uint32_t>(fx_plugins_.size()); }

    // Sends
    struct SendEntry {
        SendID   id;
        SendSlot slot;
    };
    SendID   add_send(ChannelID target, double level_db, bool pre_fader);
    void     remove_send(SendID id);
    void     set_send_level(SendID id, double db);
    std::vector<SendSlot> sends() const;

    // EQ
    BuiltInEQ& eq()             { return eq_; }
    const BuiltInEQ& eq() const { return eq_; }

    // Metering (written by audio thread, read from UI thread)
    float peak_left()  const { return peak_left_.load(); }
    float peak_right() const { return peak_right_.load(); }
    float rms_left()   const { return rms_left_.load(); }
    float rms_right()  const { return rms_right_.load(); }

    // Set metering values (called from Mixer::process on audio thread)
    void set_metering(float peak_l, float peak_r, float rms_l, float rms_r) {
        peak_left_.store(peak_l);
        peak_right_.store(peak_r);
        rms_left_.store(rms_l);
        rms_right_.store(rms_r);
    }

    // Input index for process()
    void     set_input_index(uint32_t idx) { input_index_ = idx; }
    uint32_t input_index() const           { return input_index_; }

    // DSP helpers
    double        linear_volume() const;
    static double db_to_linear(double db);
    static double linear_to_db(double linear);

protected:
    ChannelID   id_;
    std::string name_;
    ChannelType type_;

    // Level state (atomic for cross-thread reads)
    std::atomic<double> volume_db_{0.0};
    std::atomic<double> automated_volume_db_{0.0};
    std::atomic<double> pan_{0.0};
    std::atomic<bool>   muted_{false};
    std::atomic<bool>   soloed_{false};
    std::atomic<bool>   phase_invert_{false};
    std::atomic<double> width_{1.0};

    // VCA contribution (set externally by VCA engine)
    std::atomic<double> vca_contribution_db_{0.0};

    // Groups / VCA
    BusID   group_bus_   = kInvalidBusID;
    TrackID vca_track_   = kInvalidTrackID;

    // Input mapping
    uint32_t input_index_ = 0;

    // FX chain slots
    struct FXSlot {
        PluginID plugin;
        bool     bypassed = false;
    };
    std::vector<FXSlot> fx_plugins_;

    // Built-in EQ
    BuiltInEQ eq_;

    // Sends
    std::vector<SendEntry> send_entries_;
    SendID next_send_id_ = 1;

    // Metering accumulation (written by audio thread, read by UI thread)
    std::atomic<float> peak_left_{-60.0f};
    std::atomic<float> peak_right_{-60.0f};
    std::atomic<float> rms_left_{-60.0f};
    std::atomic<float> rms_right_{-60.0f};
};

// ── Free utility functions ──────────────────────────────────────
inline double db_to_linear(double db) {
    return (db <= -120.0) ? 0.0 : std::pow(10.0, db / 20.0);
}

inline double linear_to_db(double linear) {
    return (linear <= 0.0) ? -120.0 : 20.0 * std::log10(linear);
}

} // namespace aria

#endif // ARIA_MIXER_CHANNEL_H
