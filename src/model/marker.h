#ifndef ARIA_MARKER_H
#define ARIA_MARKER_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace aria {

/// Marker identifier.
struct MarkerID {
    uint64_t value = 0;

    bool operator==(const MarkerID& o) const { return value == o.value; }
    bool operator!=(const MarkerID& o) const { return value != o.value; }
};

/// A colour with 8-bit channels.
struct Color {
    uint8_t r = 0xCC;
    uint8_t g = 0xCC;
    uint8_t b = 0xCC;
};

/// A single marker on the timeline.
struct Marker {
    MarkerID    id;
    std::string name;
    uint64_t    ppqn = 0;
    Color       color;
    std::string description;
};

/// Ordered list of markers with CRUD and range queries.
class MarkerList {
public:
    /// Add a new marker at the given PPQN position.
    /// Returns the assigned ID.
    MarkerID add(const std::string& name, uint64_t ppqn);

    /// Remove a marker by ID.
    /// @return true if found and removed.
    bool remove(MarkerID id);

    /// Move a marker to a new PPQN position.
    /// @return true if found.
    bool move(MarkerID id, uint64_t new_ppqn);

    /// Rename a marker.
    /// @return true if found.
    bool rename(MarkerID id, const std::string& name);

    /// Find a marker by ID (nullptr if not found).
    Marker* find(MarkerID id);

    /// Const overload.
    const Marker* find(MarkerID id) const;

    /// Find a marker at the exact PPQN position (nullptr if none).
    Marker* find_at(uint64_t ppqn);

    /// Return all markers (sorted by PPQN).
    std::vector<Marker> all() const;

    /// Return markers within a PPQN range.
    std::vector<Marker> in_range(uint64_t start, uint64_t end) const;

    /// Number of markers.
    size_t count() const { return markers_.size(); }

private:
    std::vector<Marker> markers_;
    uint64_t next_id_ = 1;

    /// Keep markers sorted by PPQN.
    void ensure_sorted();

    /// Find iterator by ID.
    auto find_it(MarkerID id) -> decltype(markers_.begin());
    auto find_it(MarkerID id) const -> decltype(markers_.cbegin());
};

} // namespace aria

#endif // ARIA_MARKER_H
