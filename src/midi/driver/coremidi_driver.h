#ifndef ARIA_COREMIDI_DRIVER_H
#define ARIA_COREMIDI_DRIVER_H

#include "midi_driver.h"

#ifdef __APPLE__

namespace aria {

/// macOS CoreMIDI driver.
///
/// Uses the CoreMIDI framework (MIDIClientCreate, MIDIInputPortCreate,
/// MIDIOutputPortCreate, MIDISend). Compiled as Objective-C++ (.mm).
/// Handles hot-plug via MIDINotification.
class CoreMidiDriver : public MidiDriver {
public:
    CoreMidiDriver();
    ~CoreMidiDriver() override;

    CoreMidiDriver(const CoreMidiDriver&) = delete;
    CoreMidiDriver& operator=(const CoreMidiDriver&) = delete;

    // ─── MidiDriver interface ──────────────────────────────
    std::vector<MidiDeviceInfo> enumerate() override;
    bool open(const MidiDeviceInfo& dev) override;
    void close() override;
    bool send(const MidiEvent& event) override;
    bool is_open() const override { return client_ != nullptr; }

private:
    // Opaque CoreMIDI handles (pointers to MIDIClientRef, etc.)
    void* client_       = nullptr;  // MIDIClientRef
    void* input_port_   = nullptr;  // MIDIPortRef (input)
    void* output_port_  = nullptr;  // MIDIPortRef (output)
    int   input_endpoint_  = -1;    // Source endpoint index
    int   output_endpoint_ = -1;    // Destination endpoint index

    // Parse a MIDI packet list into MidiEvent(s) and dispatch.
    void process_packet_list(const void* pktlist);
};

} // namespace aria

#endif // __APPLE__

#endif // ARIA_COREMIDI_DRIVER_H
