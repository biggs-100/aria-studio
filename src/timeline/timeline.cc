#include "timeline.h"

namespace aria {

Timeline::Timeline() = default;
Timeline::~Timeline() = default;

void Timeline::set_tempo(double bpm) { bpm_ = bpm; }
double Timeline::tempo() const { return bpm_; }

void Timeline::set_time_signature(int num, int den) { ts_num_ = num; ts_den_ = den; }
void Timeline::time_signature(int& num, int& den) const { num = ts_num_; den = ts_den_; }

void Timeline::set_position(uint64_t samples) { position_ = samples; }
uint64_t Timeline::position() const { return position_; }

} // namespace aria
