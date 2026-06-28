#include "dsp_engine.h"

namespace aria {

DSPEngine::DSPEngine() = default;
DSPEngine::~DSPEngine() = default;
void DSPEngine::init(uint32_t sample_rate) { sample_rate_ = sample_rate; }

} // namespace aria
