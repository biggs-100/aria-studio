#ifndef ARIA_TEMPO_MAP_H
#define ARIA_TEMPO_MAP_H

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>

namespace aria {

/// A single tempo change event in the timeline.
struct TempoEvent {
    uint64_t sample_position = 0;  ///< Global sample position of this event.
    double   bpm             = 120.0;  ///< Beats per minute from this point.
};

/// Tempo map — a sequence of tempo events over the timeline.
///
/// Supports querying the effective BPM at any sample position.
/// Tempo events are sorted by sample_position for binary search.
///
/// Thread safety: set_events() and set_default_bpm() are intended for
/// the control (UI) thread. bpm_at() is real-time safe (no allocation,
/// no locking — reads a pre-sorted vector via binary search).
class TempoMap {
public:
    TempoMap() = default;

    /// Replace all tempo events.
    /// @param events  Unsorted list of tempo events. They will be sorted
    ///                by sample_position internally.
    void set_events(const std::vector<TempoEvent>& events);

    /// Query the effective BPM at the given sample position.
    /// Falls back to default_bpm_ if no event covers that position.
    double bpm_at(uint64_t sample_position) const;

    /// Set the default BPM (used when no tempo events are defined).
    void set_default_bpm(double bpm) { default_bpm_ = bpm; }

    /// The default BPM.
    double default_bpm() const { return default_bpm_; }

    /// Number of tempo events.
    size_t event_count() const { return events_.size(); }

    /// Access the sorted event list.
    const std::vector<TempoEvent>& events() const { return events_; }

private:
    std::vector<TempoEvent> events_;
    double default_bpm_ = 120.0;
};

} // namespace aria

#endif // ARIA_TEMPO_MAP_H
