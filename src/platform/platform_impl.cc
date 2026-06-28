#include "platform_impl.h"

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include <unistd.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

namespace aria {

std::string Platform::name() {
#ifdef _WIN32
    return "windows";
#elif __APPLE__
    return "macos";
#else
    return "linux";
#endif
}

std::string Platform::user_data_path() {
#ifdef _WIN32
    char* appdata = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata) {
        std::string path = appdata;
        free(appdata);
        return path + "/ARIA";
    }
    return "./aria_data";
#elif __APPLE__
    return std::string(getenv("HOME")) + "/Library/Application Support/ARIA";
#else
    return std::string(getenv("HOME")) + "/.config/aria";
#endif
}

std::string Platform::temp_path() {
#ifdef _WIN32
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    return std::string(tmp) + "aria";
#else
    return "/tmp/aria";
#endif
}

uint32_t Platform::cpu_core_count() {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif __APPLE__
    int count;
    size_t size = sizeof(count);
    sysctlbyname("hw.logicalcpu", &count, &size, nullptr, 0);
    return count;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

uint64_t Platform::system_ram_mb() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullTotalPhys / (1024 * 1024);
#elif __APPLE__
    int64_t mem;
    size_t size = sizeof(mem);
    sysctlbyname("hw.memsize", &mem, &size, nullptr, 0);
    return mem / (1024 * 1024);
#else
    struct sysinfo si;
    sysinfo(&si);
    return si.totalram / (1024 * 1024);
#endif
}

} // namespace aria
