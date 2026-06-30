#pragma once

#include <cstdint>

namespace aria {

/// Theme event types for EventBus integration.
/// Uses values in the 3000 range to avoid collision.
constexpr uint32_t kEventThemeChanged = 3001;

/// Event payload for theme changes.
struct ThemeChangedEvent {
    uint32_t type = kEventThemeChanged;
    uint64_t timestamp = 0;
};

namespace browser {

/// Browser event types for EventBus integration.
/// Uses numeric values in the 2000 range to avoid collision
/// with other event type ranges.
enum class BrowserEventType : uint32_t {
    kFileScanned     = 2001,
    kFileChanged     = 2002,
    kIndexProgress   = 2003,
    kSearchResult    = 2004,
    kPreviewLoaded   = 2005,
    kSearchStarted   = 2006,
    kSearchCompleted = 2007,
    kScanStarted     = 2008,
    kScanCompleted   = 2009,
    kFileDeleted     = 2010,
    kClipCreated     = 2011,
};

/// Browser event payload, compatible with EventBus::Event pattern.
struct BrowserEvent {
    BrowserEventType type;
    uint64_t         timestamp;

    // Optional payload data
    int64_t  int_value    = 0;
    double   double_value = 0.0;
    char     str_data[512] = {};
};

} // namespace browser
} // namespace aria
