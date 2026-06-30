#ifndef ARIA_SHORTCUT_MANAGER_H
#define ARIA_SHORTCUT_MANAGER_H

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace aria {

// ── KeyChord ──────────────────────────────────────────────────────

/// A keyboard shortcut identified by a key code and modifier flags.
struct KeyChord {
    int  key   = 0;   ///< Platform key code
    bool ctrl  = false;
    bool shift = false;
    bool alt   = false;
    bool meta  = false;

    bool operator==(const KeyChord& other) const noexcept {
        return key == other.key &&
               ctrl == other.ctrl &&
               shift == other.shift &&
               alt == other.alt &&
               meta == other.meta;
    }

    bool operator!=(const KeyChord& other) const noexcept {
        return !(*this == other);
    }
};

// ── ShortcutHandler ───────────────────────────────────────────────

/// Handler callback for a keyboard shortcut.
/// Returns true if the shortcut was consumed (stops propagation),
/// false to allow lower-priority handlers to fire.
using ShortcutHandler = std::function<bool()>;

// ── ShortcutEntry ─────────────────────────────────────────────────

/// A registered shortcut binding with its handler and priority.
struct ShortcutEntry {
    KeyChord       chord;
    ShortcutHandler handler;
    int            priority = 0;
};

// ── ShortcutManager ───────────────────────────────────────────────

/// Manages keyboard shortcut registration and dispatch with
/// priority-ordered propagation and conflict detection.
///
/// Registered via ServiceLocator; called by RenderLoop during the
/// input-processing phase of each frame.
class ShortcutManager {
public:
    ShortcutManager() = default;
    ~ShortcutManager() = default;

    ShortcutManager(const ShortcutManager&) = delete;
    ShortcutManager& operator=(const ShortcutManager&) = delete;

    // ── Registration ───────────────────────────────────────────

    /// Register a shortcut handler at the given priority.
    /// Higher priority values fire first. Logs a warning if the same
    /// chord is already registered (conflict).
    void register_shortcut(KeyChord chord, ShortcutHandler handler, int priority = 0);

    /// Unregister all handlers for the given chord.
    void unregister_shortcut(KeyChord chord);

    // ── Dispatch ───────────────────────────────────────────────

    /// Dispatch a key chord through all registered handlers in
    /// priority order. Stops on the first handler that returns true.
    /// Returns true if any handler consumed the shortcut.
    bool dispatch(const KeyChord& chord) const;

    // ── Conflict detection ─────────────────────────────────────

    /// Return all handlers registered for the given chord.
    /// Useful for inspecting conflicts after a warning is logged.
    std::vector<ShortcutHandler> conflicts(const KeyChord& chord) const;

private:
    std::vector<ShortcutEntry> entries_;
};

} // namespace aria

#endif // ARIA_SHORTCUT_MANAGER_H
