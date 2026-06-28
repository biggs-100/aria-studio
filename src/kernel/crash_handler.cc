#include "crash_handler.h"

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace aria {

bool CrashHandler::installed_ = false;

void CrashHandler::install() {
    if (installed_) return;
    std::signal(SIGSEGV, signal_handler);
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGFPE, signal_handler);
    std::signal(SIGILL, signal_handler);
#ifdef _WIN32
    std::signal(SIGTERM, signal_handler);
#endif
    installed_ = true;
}

void CrashHandler::uninstall() {
    if (!installed_) return;
    std::signal(SIGSEGV, SIG_DFL);
    std::signal(SIGABRT, SIG_DFL);
    std::signal(SIGFPE, SIG_DFL);
    std::signal(SIGILL, SIG_DFL);
    installed_ = false;
}

void CrashHandler::save_recovery_state() {
    std::ofstream file(recovery_file_path());
    if (file.is_open()) {
        file << "crash=true\n";
        file.close();
    }
}

bool CrashHandler::previous_session_crashed() {
    std::ifstream file(recovery_file_path());
    if (file.is_open()) {
        std::string line;
        std::getline(file, line);
        file.close();
        std::remove(recovery_file_path().c_str());
        return line == "crash=true";
    }
    return false;
}

std::string CrashHandler::recovery_file_path() {
    return "aria_recovery.txt";
}

void CrashHandler::signal_handler(int signum) {
    std::cerr << "ARIA CRASH: Signal " << signum << " received.\n";
    save_recovery_state();
    std::_Exit(EXIT_FAILURE);
}

} // namespace aria
