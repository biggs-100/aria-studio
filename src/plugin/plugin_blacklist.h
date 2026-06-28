#ifndef ARIA_PLUGIN_PLUGIN_BLACKLIST_H
#define ARIA_PLUGIN_PLUGIN_BLACKLIST_H

#include "audio_plugin_types.h"

#include <chrono>
#include <string>
#include <unordered_map>

namespace aria::plugin {

/// Escalation levels for plugin blacklisting.
enum class BlacklistLevel {
    None,       ///< Not blacklisted — plugin loads normally
    Warning,    ///< Plugin loads but shows a warning (1 crash)
    Disabled,   ///< Plugin skipped during scan (3+ crashes)
    Banned      ///< Manually banned — never loads (5+ crashes or manual)
};

/// Persistent record for a blacklisted plugin.
struct BlacklistEntry {
    PluginID id;                                    ///< Plugin identifier
    BlacklistLevel level = BlacklistLevel::None;    ///< Current escalation level
    int crash_count = 0;                            ///< Total crash count
    std::string reason;                             ///< Human-readable reason
    std::chrono::system_clock::time_point last_crash; ///< Timestamp of last crash
};

/// Tracks plugin crashes and manages escalation to Disabled/Banned.
///
/// Escalation rules:
///   1 crash  → Warning
///   3 crashes → Disabled
///   5+ crashes → Banned
///
/// The blacklist persists to and loads from a JSON file
/// (typically ~/.aria/blacklist.json).
class PluginBlacklist {
public:
    PluginBlacklist() = default;
    ~PluginBlacklist() = default;

    // ── Mutators ───────────────────────────────────────────────

    /// Report a crash for the given plugin. Escalates level automatically.
    void report_crash(const PluginID& id);

    /// Manually ban a plugin (overrides auto escalation).
    void ban(const PluginID& id, const std::string& reason);

    /// Remove a ban or escalation — clears the entry entirely.
    void unban(const PluginID& id);

    /// Clear all blacklist entries.
    void clear();

    /// Clear a single entry.
    void clear_entry(const PluginID& id);

    // ── Queries ────────────────────────────────────────────────

    /// Get the current blacklist level for a plugin.
    BlacklistLevel level(const PluginID& id) const;

    /// Returns true if the plugin is Disabled or Banned.
    bool is_blacklisted(const PluginID& id) const;

    /// Get the full entry (for display). Returns nullopt if not found.
    std::optional<BlacklistEntry> entry(const PluginID& id) const;

    /// Get all entries.
    const std::unordered_map<PluginID, BlacklistEntry>& entries() const;

    // ── Persistence ────────────────────────────────────────────

    /// Save blacklist to a JSON file. Returns true on success.
    bool save(const std::string& path) const;

    /// Load blacklist from a JSON file. Corrupt or missing files
    /// initialise empty without crashing. Returns true on success.
    bool load(const std::string& path);

private:
    /// Determine the BlacklistLevel from a crash count.
    static BlacklistLevel level_from_crash_count(int crashes);

    std::unordered_map<PluginID, BlacklistEntry> entries_;
};

} // namespace aria::plugin

#endif // ARIA_PLUGIN_PLUGIN_BLACKLIST_H
