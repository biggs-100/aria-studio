#include "plugin_blacklist.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <ctime>
inline time_t timegm(struct tm* tm) {
    return _mkgmtime(tm);
}
#endif

namespace aria::plugin {

using json = nlohmann::json;
namespace chrono = std::chrono;

// ══════════════════════════════════════════════════════════════════════
//  Escalation
// ══════════════════════════════════════════════════════════════════════

BlacklistLevel PluginBlacklist::level_from_crash_count(int crashes) {
    if (crashes >= 5) return BlacklistLevel::Banned;
    if (crashes >= 3) return BlacklistLevel::Disabled;
    if (crashes >= 1) return BlacklistLevel::Warning;
    return BlacklistLevel::None;
}

// ══════════════════════════════════════════════════════════════════════
//  Mutators
// ══════════════════════════════════════════════════════════════════════

void PluginBlacklist::report_crash(const PluginID& id) {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        BlacklistEntry entry;
        entry.id = id;
        entry.crash_count = 1;
        entry.level = level_from_crash_count(1);
        entry.last_crash = chrono::system_clock::now();
        entries_[id] = entry;
        return;
    }

    it->second.crash_count++;
    it->second.level = level_from_crash_count(it->second.crash_count);
    it->second.last_crash = chrono::system_clock::now();

    // Only update reason for auto-escalation if not manually banned
    if (it->second.level != BlacklistLevel::Banned) {
        switch (it->second.level) {
            case BlacklistLevel::Warning:
                it->second.reason = "Plugin crashed once — monitoring";
                break;
            case BlacklistLevel::Disabled:
                it->second.reason = "Plugin crashed 3 times — disabled";
                break;
            case BlacklistLevel::Banned:
                it->second.reason = "Plugin crashed 5+ times — banned";
                break;
            default:
                break;
        }
    }
}

void PluginBlacklist::ban(const PluginID& id, const std::string& reason) {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        BlacklistEntry entry;
        entry.id = id;
        entry.level = BlacklistLevel::Banned;
        entry.crash_count = 0;
        entry.reason = reason;
        entry.last_crash = chrono::system_clock::now();
        entries_[id] = entry;
        return;
    }

    it->second.level = BlacklistLevel::Banned;
    it->second.reason = reason;
}

void PluginBlacklist::unban(const PluginID& id) {
    entries_.erase(id);
}

void PluginBlacklist::clear() {
    entries_.clear();
}

void PluginBlacklist::clear_entry(const PluginID& id) {
    entries_.erase(id);
}

// ══════════════════════════════════════════════════════════════════════
//  Queries
// ══════════════════════════════════════════════════════════════════════

BlacklistLevel PluginBlacklist::level(const PluginID& id) const {
    auto it = entries_.find(id);
    if (it == entries_.end()) return BlacklistLevel::None;
    return it->second.level;
}

bool PluginBlacklist::is_blacklisted(const PluginID& id) const {
    auto lvl = level(id);
    return lvl == BlacklistLevel::Disabled || lvl == BlacklistLevel::Banned;
}

std::optional<BlacklistEntry> PluginBlacklist::entry(const PluginID& id) const {
    auto it = entries_.find(id);
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

const std::unordered_map<PluginID, BlacklistEntry>& PluginBlacklist::entries() const {
    return entries_;
}

// ══════════════════════════════════════════════════════════════════════
//  Persistence — JSON
// ══════════════════════════════════════════════════════════════════════

bool PluginBlacklist::save(const std::string& path) const {
    json j = json::object();

    for (const auto& [id, entry] : entries_) {
        json item = json::object();
        item["id"] = entry.id;
        item["level"] = static_cast<int>(entry.level);
        item["crash_count"] = entry.crash_count;
        item["reason"] = entry.reason;

        // Serialise timestamp as ISO 8601
        auto time_t = chrono::system_clock::to_time_t(entry.last_crash);
        std::tm tm{};
#if defined(_WIN32)
        gmtime_s(&tm, &time_t);
#else
        gmtime_r(&time_t, &tm);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        item["last_crash"] = buf;

        j[id] = item;
    }

    // Atomic write: write to temp, then rename
    std::string tmp_path = path + ".tmp";
    {
        std::ofstream ofs(tmp_path);
        if (!ofs.is_open()) return false;
        ofs << j.dump(2);
        ofs.close();
    }

    if (std::rename(tmp_path.c_str(), path.c_str()) != 0) {
        // Remove temp on failure
        std::remove(tmp_path.c_str());
        return false;
    }

    return true;
}

bool PluginBlacklist::load(const std::string& path) {
    entries_.clear();

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        // Missing file = empty blacklist, not an error
        return true;
    }

    json j;
    try {
        ifs >> j;
    } catch (...) {
        // Corrupt JSON — initialise empty
        return false;
    }

    if (!j.is_object()) return false;

    for (auto it = j.begin(); it != j.end(); ++it) {
        try {
            BlacklistEntry entry;
            entry.id = it.key();
            entry.level = static_cast<BlacklistLevel>(
                it->value("level", static_cast<int>(BlacklistLevel::None)));
            entry.crash_count = it->value("crash_count", 0);
            entry.reason = it->value("reason", "");

            // Parse timestamp if present
            std::string ts = it->value("last_crash", "");
            if (!ts.empty()) {
                // Simple ISO 8601 parse: "2026-06-27T12:00:00Z"
                std::tm tm{};
#if defined(_WIN32)
                sscanf_s(ts.c_str(), "%d-%d-%dT%d:%d:%dZ",
                         &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                         &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
#else
                std::sscanf(ts.c_str(), "%d-%d-%dT%d:%d:%dZ",
                            &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                            &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
#endif
                tm.tm_year -= 1900;
                tm.tm_mon -= 1;
                time_t t = timegm(&tm);
                entry.last_crash = chrono::system_clock::from_time_t(t);
            }

            entries_[entry.id] = entry;
        } catch (...) {
            // Skip malformed entries
            continue;
        }
    }

    return true;
}

} // namespace aria::plugin
