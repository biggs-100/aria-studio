#include "platform_driver.h"

// ─── Platform Includes (OUTSIDE namespace — must not pollute aria::std) ──

#if defined(_WIN32)
#  include "asio_driver.h"
#  include "wasapi_driver.h"
#elif defined(__APPLE__)
#  include "coreaudio_driver.h"
#elif defined(__linux__)
#  include "pipewire_driver.h"
#  include "alsa_driver.h"
#endif

namespace aria {

// ─── Factory Implementation ────────────────────────────────────

std::unique_ptr<AudioDriver> PlatformDriver::create_default() {
#if defined(_WIN32)
    // Try ASIO first; fall back to WASAPI.
    auto asio = std::make_unique<AsioDriver>();
    if (!asio->enumerate_devices().empty()) {
        return asio;
    }
    return std::make_unique<WasapiDriver>();
#elif defined(__APPLE__)
    return std::make_unique<CoreAudioDriver>();
#elif defined(__linux__)
    // Try PipeWire first; fall back to ALSA.
    auto pw = std::make_unique<PipeWireDriver>();
    if (!pw->enumerate_devices().empty()) {
        return pw;
    }
    return std::make_unique<AlsaDriver>();
#else
    return nullptr;
#endif
}

std::unique_ptr<AudioDriver> PlatformDriver::create_by_name(
    const std::string& name)
{
#if defined(_WIN32)
    if (name == "asio")   return std::make_unique<AsioDriver>();
    if (name == "wasapi") return std::make_unique<WasapiDriver>();
#elif defined(__APPLE__)
    if (name == "coreaudio") return std::make_unique<CoreAudioDriver>();
#elif defined(__linux__)
    if (name == "pipewire") return std::make_unique<PipeWireDriver>();
    if (name == "alsa")     return std::make_unique<AlsaDriver>();
#endif
    return nullptr;
}

std::vector<std::string> PlatformDriver::available_drivers() {
    std::vector<std::string> drivers;
#if defined(_WIN32)
    drivers.emplace_back("asio");
    drivers.emplace_back("wasapi");
#elif defined(__APPLE__)
    drivers.emplace_back("coreaudio");
#elif defined(__linux__)
    drivers.emplace_back("pipewire");
    drivers.emplace_back("alsa");
#endif
    return drivers;
}

} // namespace aria
