#ifndef ARIA_CRASH_HANDLER_H
#define ARIA_CRASH_HANDLER_H

#include <string>

namespace aria {

/// Crash handler — signal/exception capture and recovery state save.
class CrashHandler {
public:
    static void install();
    static void uninstall();

    /// Save emergency recovery state.
    static void save_recovery_state();

    /// Check if previous session crashed.
    static bool previous_session_crashed();

    /// Get recovery file path.
    static std::string recovery_file_path();

private:
    static bool installed_;

    static void signal_handler(int signum);
};

} // namespace aria

#endif // ARIA_CRASH_HANDLER_H
