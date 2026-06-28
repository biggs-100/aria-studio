#ifndef ARIA_VIRTUAL_PORT_DRIVER_H
#define ARIA_VIRTUAL_PORT_DRIVER_H

#include "midi_driver.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>

namespace aria {

/// Virtual MIDI port driver for inter-application routing.
///
/// Creates a platform-virtual MIDI port that other applications can
/// discover and connect to. On Windows this uses a loopback via
/// midiOutOpen with a hidden window. On macOS it uses
/// MIDIExternalDeviceCreate. On Linux it uses snd_seq_create_simple_port.
///
/// The driver also maintains an internal loopback queue so events
/// sent via send() can be read back via the receive callback.
class VirtualPortDriver : public MidiDriver {
public:
    VirtualPortDriver();
    ~VirtualPortDriver() override;

    VirtualPortDriver(const VirtualPortDriver&) = delete;
    VirtualPortDriver& operator=(const VirtualPortDriver&) = delete;

    // ─── MidiDriver interface ──────────────────────────────
    std::vector<MidiDeviceInfo> enumerate() override;
    bool open(const MidiDeviceInfo& dev) override;
    void close() override;
    bool send(const MidiEvent& event) override;
    bool is_open() const override { return virtual_port_open_; }

    /// Create a virtual port with the given name.
    bool create_virtual_port(const std::string& port_name);

    /// Remove a previously created virtual port.
    void remove_virtual_port();

    /// List available virtual ports (including those created by other apps).
    std::vector<MidiDeviceInfo> enumerate_virtual_ports() const;

private:
    std::string port_name_;
    bool virtual_port_open_ = false;

    // Internal loopback queue
    std::mutex queue_mutex_;
    std::queue<MidiEvent> loopback_queue_;

    // Polling thread for internal loopback
    std::thread loopback_thread_;
    std::atomic<bool> loopback_running_{false};

    void loopback_poll();

    // Platform-specific handles
#ifdef _WIN32
    void* internal_out_handle_ = nullptr;   // HMIDIOUT for loopback
    void* internal_in_handle_  = nullptr;   // HMIDIIN for virtual in
#endif
};

} // namespace aria

#endif // ARIA_VIRTUAL_PORT_DRIVER_H
