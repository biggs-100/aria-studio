#ifndef ARIA_MIXER_ROUTING_H
#define ARIA_MIXER_ROUTING_H

#include <cstdint>
#include <string>

#include "mixer/channel.h"

namespace aria {

// ── Route Target ────────────────────────────────────────────────
struct RouteTarget {
    enum Type : uint8_t {
        Master,     // Send to master bus
        Bus,        // Send to a specific bus
        Track,      // Send directly to a track
        External,   // Hardware output
        None        // No output (silent)
    };

    Type type;
    union {
        BusID    bus_id;
        TrackID  track_id;
        uint32_t external_channel;  // Hardware output channel index
    };

    // Enum for mono/stereo routing
    enum class Format : uint8_t { Stereo, Mono, DualMono };
    Format format;

    RouteTarget() : type(Master), bus_id(0), format(Format::Stereo) {}
};

// ── Audio Input ─────────────────────────────────────────────────
struct AudioInput {
    enum Type : uint8_t {
        None,
        Internal,       // Internal track audio
        ExternalAudio,  // External audio device input
        ExternalMIDI,   // External MIDI input
        Sidechain       // Sidechain input from another channel
    };

    Type        type      = None;
    uint32_t    channel   = 0;       // Channel number within the device
    std::string device_id;           // Device identifier for external inputs
};

} // namespace aria

#endif // ARIA_MIXER_ROUTING_H
