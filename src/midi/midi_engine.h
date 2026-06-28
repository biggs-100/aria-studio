#ifndef ARIA_MIDI_ENGINE_H
#define ARIA_MIDI_ENGINE_H

#include <cstdint>

namespace aria {

/// MIDI Engine — MIDI I/O, recording, and playback.
class MidiEngine {
public:
    MidiEngine();
    ~MidiEngine();

    bool init();
    void shutdown();
    bool is_initialized() const;

private:
    bool initialized_ = false;
};

} // namespace aria

#endif // ARIA_MIDI_ENGINE_H
