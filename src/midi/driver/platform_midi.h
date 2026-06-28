#ifndef ARIA_PLATFORM_MIDI_H
#define ARIA_PLATFORM_MIDI_H

#include "midi_driver.h"
#include <memory>
#include <string>

namespace aria {

/// Platform MIDI factory.
///
/// Creates the appropriate MIDI driver for the current platform.
/// Automatically selects WinMM on Windows, CoreMIDI on macOS,
/// and ALSA Sequencer on Linux. Virtual port driver is available
/// on all platforms.
struct PlatformMidi {
    /// Create the default platform MIDI driver.
    static std::unique_ptr<MidiDriver> create_default_driver();

    /// Create a virtual port driver.
    static std::unique_ptr<MidiDriver> create_virtual_port_driver();

    /// Create a driver by platform name ("winmm", "coremidi", "alsa", "virtual").
    static std::unique_ptr<MidiDriver> create_driver(const std::string& name);

    /// Get a human-readable name for the current platform's driver.
    static std::string default_driver_name();
};

} // namespace aria

#endif // ARIA_PLATFORM_MIDI_H
