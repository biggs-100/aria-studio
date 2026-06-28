#ifndef ARIA_DSP_ENGINE_H
#define ARIA_DSP_ENGINE_H

#include <cstdint>

namespace aria {

/// DSP Engine — SIMD-accelerated signal processing library.
class DSPEngine {
public:
    DSPEngine();
    ~DSPEngine();

    void init(uint32_t sample_rate);

private:
    uint32_t sample_rate_ = 48000;
};

} // namespace aria

#endif // ARIA_DSP_ENGINE_H
