#include "track.h"

#include <algorithm>

namespace aria {

namespace {
    uint64_t next_id = 1;
}

Track::Track(TrackType type)
    : id_(next_id++), type_(type) {}

TrackID Track::id() const { return id_; }
void Track::set_name(const std::string& name) { name_ = name; }
std::string Track::name() const { return name_; }
TrackType Track::type() const { return type_; }

void Track::set_volume(double db) { volume_db_ = db; }
double Track::volume() const { return volume_db_; }
void Track::set_pan(double pan) { pan_ = pan; }
double Track::pan() const { return pan_; }

void Track::set_muted(bool muted) { muted_ = muted; }
bool Track::is_muted() const { return muted_; }
void Track::set_soloed(bool soloed) { soloed_ = soloed; }
bool Track::is_soloed() const { return soloed_; }

// ─── Clip management ────────────────────────────────────────

bool Track::add_clip(std::shared_ptr<Clip> clip, uint64_t start_ppqn) {
    if (frozen_) return false;
    ClipPlacement cp;
    cp.clip = std::move(clip);
    cp.start_ppqn = start_ppqn;
    clips_.push_back(std::move(cp));
    return true;
}

// ─── Freeze state ──────────────────────────────────────────

void Track::set_frozen(bool frozen) { frozen_ = frozen; }
bool Track::is_frozen() const { return frozen_; }

void Track::set_show_frozen_audio(bool show) { show_frozen_ = show; }
bool Track::show_frozen_audio() const { return show_frozen_; }

void Track::set_frozen_clip(std::shared_ptr<Clip> clip) {
    frozen_clip_ = std::move(clip);
}
Clip* Track::frozen_clip() const { return frozen_clip_.get(); }

void Track::set_freeze_dirty(bool dirty) { freeze_dirty_ = dirty; }
bool Track::is_freeze_dirty() const { return freeze_dirty_; }

Clip* Track::clip_at(uint64_t ppqn) const {
    for (const auto& cp : clips_) {
        uint64_t end = cp.start_ppqn + cp.clip->length();
        if (ppqn >= cp.start_ppqn && ppqn < end) {
            return cp.clip.get();
        }
    }
    return nullptr;
}

// ─── Routing ────────────────────────────────────────────────

void Track::set_routing_out(const RouteTarget& target) {
    routing_out_ = target;
}

const RouteTarget& Track::routing_out() const {
    return routing_out_;
}

void Track::add_send(const TrackSendSlot& send) {
    sends_.push_back(send);
}

const std::vector<TrackSendSlot>& Track::sends() const {
    return sends_;
}

void Track::clear_sends() {
    sends_.clear();
}

} // namespace aria
