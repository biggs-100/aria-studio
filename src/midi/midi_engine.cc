#include "midi_engine.h"

namespace aria {

MidiEngine::MidiEngine() = default;
MidiEngine::~MidiEngine() { shutdown(); }

bool MidiEngine::init() {
    if (initialized_) return true;

    // Create default platform driver
    driver_ = PlatformMidi::create_default_driver();
    if (!driver_) {
        // Non-fatal — engine can operate without a driver
        driver_ = PlatformMidi::create_virtual_port_driver();
    }

    initialized_ = true;
    return true;
}

void MidiEngine::shutdown() {
    if (driver_) {
        driver_->close();
        driver_.reset();
    }
    driver_callback_attached_ = false;
    recorder_.reset();
    mpe_decoder_.reset();
    mpe_encoder_.reset();
    initialized_ = false;
}

// ─── MIDI Drivers ───────────────────────────────────────────────

bool MidiEngine::open_input_device(const MidiDeviceInfo& dev) {
    if (!driver_) {
        driver_ = PlatformMidi::create_default_driver();
    }

    if (!driver_->open(dev)) {
        return false;
    }

    // Attach receive callback
    if (!driver_callback_attached_) {
        driver_->set_callback(
            [this](const MidiEvent& event) { on_driver_event(event); });
        driver_callback_attached_ = true;
    }

    return true;
}

bool MidiEngine::open_output_device(const MidiDeviceInfo& dev) {
    if (!driver_) {
        driver_ = PlatformMidi::create_default_driver();
    }

    return driver_->open(dev);
}

void MidiEngine::close_driver() {
    if (driver_) {
        driver_->close();
    }
    driver_callback_attached_ = false;
}

std::vector<MidiDeviceInfo> MidiEngine::enumerate_devices() const {
    if (driver_) {
        return driver_->enumerate();
    }
    return {};
}

// ─── Recorder ───────────────────────────────────────────────────

void MidiEngine::start_recording(uint32_t ppqn_position) {
    recorder_.start_recording(ppqn_position);
}

void MidiEngine::stop_recording() {
    if (recorder_.is_recording()) {
        recorded_clip_ = recorder_.finalize_clip();
    }
}

// ─── Process ────────────────────────────────────────────────────

void MidiEngine::process(uint32_t frames, uint64_t sample_position, double bpm) {
    if (!initialized_) return;

    // Step 1: Process MIDI clock (generates ticks based on BPM)
    clock_.process(frames, bpm);

    // Step 2: Process MIDI graph (routes events through nodes)
    graph_.process(frames, sample_position);
}

void MidiEngine::process_incoming_event(const MidiEvent& event,
                                        uint64_t sample_position,
                                        double sample_rate) {
    // Step 1: Decode MPE
    mpe_decoder_.process(event);

    // Step 2: Track notes
    if (event.type == MidiMessageType::NoteOn && event.data2 > 0) {
        note_tracker_.note_on(event.data1, event.channel, event.data2,
                              sample_position);
    } else if (event.type == MidiMessageType::NoteOff ||
               (event.type == MidiMessageType::NoteOn && event.data2 == 0)) {
        note_tracker_.note_off(event.data1, event.channel, sample_position);
    } else if (event.is_cc() && event.data1 == 64) {
        // Sustain pedal
        note_tracker_.sustain(event.data2 >= 64);
    }

    // Step 3: Record if active
    if (recorder_.is_recording()) {
        recorder_.record_event(event, sample_position, sample_rate);
    }
}

// ─── Internal ───────────────────────────────────────────────────

void MidiEngine::on_driver_event(const MidiEvent& event) {
    // Called from the driver's I/O thread.
    // For now, we queue the event — in a real implementation,
    // this would go through a lock-free queue to the audio thread.
    process_incoming_event(event, 0, 48000.0);
}

} // namespace aria
