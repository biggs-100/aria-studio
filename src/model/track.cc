#include "track.h"

namespace aria {

namespace {
    uint64_t next_id = 1;
}

Track::Track(TrackType type)
    : id_(next_id++), type_(type) {}

uint64_t Track::id() const { return id_; }
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

} // namespace aria
