#ifndef ARIA_MIDI_DRIVER_H
#define ARIA_MIDI_DRIVER_H

#include "midi/midi_types.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace aria {

// ─── MidiDeviceInfo ─────────────────────────────────────────────

/// Information about a detected MIDI device (physical or virtual).
struct MidiDeviceInfo {
    enum class Direction : uint8_t { Input, Output, Duplex };
    enum class Type : uint8_t { Physical, Virtual, USB };

    std::string id;        ///< Platform-unique identifier.
    std::string name;      ///< Human-readable device name.
    Direction   direction  = Direction::Input;
    Type        type       = Type::Physical;
    bool        is_default = false;
};

// ─── MidiDriver (abstract base) ─────────────────────────────────

/// Abstract interface for platform-specific MIDI I/O drivers.
///
/// Subclasses implement platform-specific enumeration, opening,
/// sending, and receiving MIDI events. The receive callback is
/// called from a platform I/O thread — implementations must
/// handle thread safety appropriately.
class MidiDriver {
public:
    virtual ~MidiDriver() = default;

    /// Enumerate available MIDI devices.
    virtual std::vector<MidiDeviceInfo> enumerate() = 0;

    /// Open a MIDI device for I/O.
    virtual bool open(const MidiDeviceInfo& dev) = 0;

    /// Close the currently open device.
    virtual void close() = 0;

    /// Send a single MIDI event to the open device.
    virtual bool send(const MidiEvent& event) = 0;

    /// The driver is ready for send/receive.
    virtual bool is_open() const = 0;

    // ─── Callback ─────────────────────────────────────────
    using ReceiveCallback = std::function<void(const MidiEvent&)>;

    /// Set the callback for incoming MIDI events.
    /// Called from the platform I/O thread.
    void set_callback(ReceiveCallback cb) { callback_ = std::move(cb); }

    /// Last error message for diagnostics.
    std::string last_error() const { return last_error_; }

    /// Clear the last error.
    void clear_error() { last_error_.clear(); }

protected:
    ReceiveCallback callback_;
    std::string     last_error_;

public:
    /// Helper to dispatch a received event through the callback.
    /// Public so platform callback thunks can access it.
    void dispatch_receive(const MidiEvent& event) {
        if (callback_) {
            callback_(event);
        }
    }
};

} // namespace aria

#endif // ARIA_MIDI_DRIVER_H
