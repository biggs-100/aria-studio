#include "graphics/shortcut_manager.h"

#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>

namespace aria {

// =====================================================================
// Registration
// =====================================================================

void ShortcutManager::register_shortcut(KeyChord chord, ShortcutHandler handler, int priority) {
    // Check for conflicts (existing entries with the same chord)
    auto existing = conflicts(chord);
    if (!existing.empty()) {
        // Log a warning about the duplicate registration
        std::fprintf(stderr,
            "[ShortcutManager] Warning: duplicate shortcut registration for "
            "key=%d ctrl=%d shift=%d alt=%d meta=%d\n",
            chord.key,
            static_cast<int>(chord.ctrl),
            static_cast<int>(chord.shift),
            static_cast<int>(chord.alt),
            static_cast<int>(chord.meta));
    }

    entries_.push_back({chord, std::move(handler), priority});

    // Sort by priority descending so dispatch always fires highest first.
    std::stable_sort(entries_.begin(), entries_.end(),
        [](const ShortcutEntry& a, const ShortcutEntry& b) {
            return a.priority > b.priority;
        });
}

void ShortcutManager::unregister_shortcut(KeyChord chord) {
    auto it = std::remove_if(entries_.begin(), entries_.end(),
        [&](const ShortcutEntry& e) { return e.chord == chord; });
    entries_.erase(it, entries_.end());
}

// =====================================================================
// Dispatch
// =====================================================================

bool ShortcutManager::dispatch(const KeyChord& chord) const {
    for (const auto& entry : entries_) {
        if (entry.chord == chord) {
            if (entry.handler && entry.handler()) {
                return true;  // Consumed
            }
        }
    }
    return false;  // Not consumed
}

// =====================================================================
// Conflict detection
// =====================================================================

std::vector<ShortcutHandler> ShortcutManager::conflicts(const KeyChord& chord) const {
    std::vector<ShortcutHandler> result;
    for (const auto& entry : entries_) {
        if (entry.chord == chord) {
            result.push_back(entry.handler);
        }
    }
    return result;
}

} // namespace aria
