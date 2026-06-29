#include "routing.h"

#include <algorithm>

namespace aria {

// ─── RoutingManager ───────────────────────────────────────────

void RoutingManager::set_route(TrackID source, const RouteTarget& target) {
    routes_[source] = target;
}

RouteTarget RoutingManager::get_route(TrackID source) const {
    auto it = routes_.find(source);
    if (it != routes_.end()) return it->second;
    return RouteTarget{};  // default: Master
}

void RoutingManager::add_send(TrackID source, const TrackSendSlot& send) {
    sends_[source].push_back(send);
}

void RoutingManager::remove_send(TrackID source, TrackID target) {
    auto it = sends_.find(source);
    if (it == sends_.end()) return;

    auto& vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [target](const TrackSendSlot& s) {
            return s.target.target_id == target;
        }), vec.end());
}

std::vector<TrackSendSlot> RoutingManager::get_sends(TrackID source) const {
    auto it = sends_.find(source);
    if (it != sends_.end()) return it->second;
    return {};
}

std::vector<TrackID> RoutingManager::get_sources_for_destination(
    TrackID dest) const {
    std::vector<TrackID> sources;
    for (const auto& [src, sends] : sends_) {
        for (const auto& send : sends) {
            if (send.target.target_id == dest) {
                sources.push_back(src);
                break;
            }
        }
    }
    return sources;
}

void RoutingManager::disconnect_track(TrackID id) {
    routes_.erase(id);
    sends_.erase(id);
}

void RoutingManager::clear() {
    routes_.clear();
    sends_.clear();
}

} // namespace aria
