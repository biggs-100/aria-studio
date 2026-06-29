#ifndef ARIA_MODEL_ROUTING_H
#define ARIA_MODEL_ROUTING_H

#include "model/types.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace aria {

// ─── RouteTarget ──────────────────────────────────────────────

/// Destination type for a route.
enum class RouteType : uint8_t {
    Master,   ///< Route to master output
    Bus,      ///< Route to a bus/group
    Track,    ///< Route to an individual track
    External  ///< Route to an external hardware output
};

/// A routing destination — where the signal goes.
struct RouteTarget {
    RouteType type = RouteType::Master;
    TrackID   target_id{};  ///< Target track/bus ID (if applicable)
    std::string name;        ///< Display name for the route

    bool operator==(const RouteTarget& o) const {
        return type == o.type && target_id == o.target_id;
    }
    bool operator!=(const RouteTarget& o) const {
        return !(*this == o);
    }
};

// ─── TrackSendSlot ────────────────────────────────────────────

/// An individual send from a track to a bus/track/master.
struct TrackSendSlot {
    RouteTarget target;
    double      level_db = 0.0;  ///< Send level in dB
    bool        pre_fader = false;
    bool        pre_fx    = false;

    bool is_active() const { return target.target_id.value != 0 ||
                                    target.type != RouteType::Master; }
};

// ─── RoutingManager ───────────────────────────────────────────

/// Manages routing connections between tracks.
///
/// Provides a global view of all routes: where each track sends its
/// signal and which tracks send to each bus/group.
class RoutingManager {
public:
    RoutingManager() = default;
    ~RoutingManager() = default;

    /// Set the main routing output for a track.
    void set_route(TrackID source, const RouteTarget& target);

    /// Get the main routing output for a track.
    RouteTarget get_route(TrackID source) const;

    /// Add a send from source to target with the given level.
    void add_send(TrackID source, const TrackSendSlot& send);

    /// Remove all sends from source to target.
    void remove_send(TrackID source, TrackID target);

    /// Get all sends from a given source track.
    std::vector<TrackSendSlot> get_sends(TrackID source) const;

    /// Get all tracks that send to a given destination (for bus summing).
    std::vector<TrackID> get_sources_for_destination(TrackID dest) const;

    /// Remove all routes and sends for a given track.
    void disconnect_track(TrackID id);

    /// Clear all routes and sends.
    void clear();

private:
    std::unordered_map<TrackID, RouteTarget> routes_;
    std::unordered_map<TrackID, std::vector<TrackSendSlot>> sends_;
};

} // namespace aria

#endif // ARIA_MODEL_ROUTING_H
