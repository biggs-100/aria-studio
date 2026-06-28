#ifndef ARIA_PLATFORM_IMPL_H
#define ARIA_PLATFORM_IMPL_H

#include <string>

namespace aria {

/// Platform-specific utilities.
struct Platform {
    /// Get the platform name.
    static std::string name();

    /// Get the user data directory.
    static std::string user_data_path();

    /// Get the system temporary directory.
    static std::string temp_path();

    /// Get number of CPU cores.
    static uint32_t cpu_core_count();

    /// Get total system RAM in MB.
    static uint64_t system_ram_mb();
};

} // namespace aria

#endif // ARIA_PLATFORM_IMPL_H
