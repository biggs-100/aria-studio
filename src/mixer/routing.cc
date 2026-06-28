#include "mixer/routing.h"

namespace aria {

// Routing types are primarily data structures used by Mixer.
// No complex implementation logic in Slice 1 — all behavior
// is embedded in Mixer::process() and Channel send management.
//
// Future slices will add:
//   - SendManager with dedicated send processing
//   - ReturnChannel class
//   - External hardware routing

} // namespace aria
