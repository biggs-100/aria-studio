#ifndef ARIA_PLATFORM_DRIVER_H
#define ARIA_PLATFORM_DRIVER_H

#include "audio_driver.h"
#include <memory>
#include <string>
#include <vector>

namespace aria {

/// Factory for creating platform-native audio driver instances.
///
/// Typical usage:
///   auto driver = PlatformDriver::create_default();
///   auto devs = driver->enumerate_devices();
///   if (!devs.empty()) driver->open(devs[0], 48000, 128);
class PlatformDriver {
public:
    /// Create the best available driver for the current platform.
    /// On Windows: tries ASIO first, falls back to WASAPI.
    /// On macOS: returns CoreAudio.
    /// On Linux: tries PipeWire first, falls back to ALSA.
    static std::unique_ptr<AudioDriver> create_default();

    /// Create a driver by name ("asio", "wasapi", "coreaudio", "alsa",
    /// "pipewire"). Returns nullptr if the name is not recognized or
    /// the platform does not support the requested driver type.
    static std::unique_ptr<AudioDriver> create_by_name(
        const std::string& name);

    /// List all available driver names for the current platform.
    static std::vector<std::string> available_drivers();
};

} // namespace aria

#endif // ARIA_PLATFORM_DRIVER_H
