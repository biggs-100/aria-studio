#include "platform_midi.h"

#if defined(_WIN32)
#  include "winmm_driver.h"
#elif defined(__APPLE__)
#  include "coremidi_driver.h"
#else
#  include "alsa_seq_driver.h"
#endif

#include "virtual_port_driver.h"

namespace aria {

std::unique_ptr<MidiDriver> PlatformMidi::create_default_driver() {
#if defined(_WIN32)
    return std::make_unique<WinMMDriver>();
#elif defined(__APPLE__)
    return std::make_unique<CoreMidiDriver>();
#else
    return std::make_unique<AlsaSeqDriver>();
#endif
}

std::unique_ptr<MidiDriver> PlatformMidi::create_virtual_port_driver() {
    return std::make_unique<VirtualPortDriver>();
}

std::unique_ptr<MidiDriver> PlatformMidi::create_driver(const std::string& name) {
    if (name == "virtual") {
        return create_virtual_port_driver();
    }
#if defined(_WIN32)
    if (name == "winmm") return std::make_unique<WinMMDriver>();
#elif defined(__APPLE__)
    if (name == "coremidi") return std::make_unique<CoreMidiDriver>();
#else
    if (name == "alsa") return std::make_unique<AlsaSeqDriver>();
#endif
    return create_default_driver();
}

std::string PlatformMidi::default_driver_name() {
#if defined(_WIN32)
    return "winmm";
#elif defined(__APPLE__)
    return "coremidi";
#else
    return "alsa";
#endif
}

} // namespace aria
