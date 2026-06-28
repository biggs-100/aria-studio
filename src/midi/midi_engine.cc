#include "midi_engine.h"

namespace aria {

MidiEngine::MidiEngine() = default;
MidiEngine::~MidiEngine() { shutdown(); }

bool MidiEngine::init() { initialized_ = true; return true; }
void MidiEngine::shutdown() { initialized_ = false; }
bool MidiEngine::is_initialized() const { return initialized_; }

} // namespace aria
