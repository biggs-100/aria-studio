#include "audio_node.h"

namespace aria {

// Default parameter implementation — does nothing, override in subclasses.
void AudioNode::set_parameter(uint32_t /*index*/, double /*value*/) {}

double AudioNode::get_parameter(uint32_t /*index*/) const { return 0.0; }

} // namespace aria
