#ifndef ARIA_MIDI_ENGINE_H
#define ARIA_MIDI_ENGINE_H

#include "midi_types.h"
#include "midi_clip.h"
#include "note_tracker.h"
#include "midi_graph.h"
#include "midi_clock.h"
#include "midi_recorder.h"
#include "mpe.h"
#include "driver/midi_driver.h"
#include "driver/platform_midi.h"
#include <cstdint>
#include <memory>

namespace aria {

/// MIDI Engine — MIDI I/O, recording, playback, and processing.
///
/// Central coordinator for the MIDI subsystem. Owns the MIDI graph,
/// clock, note tracker, MIDI drivers, recorder, and MPE decoder.
/// Integrates with the audio engine via the process() method,
/// called from the audio thread.
class MidiEngine {
public:
    MidiEngine();
    ~MidiEngine();

    MidiEngine(const MidiEngine&) = delete;
    MidiEngine& operator=(const MidiEngine&) = delete;

    /// Initialize the MIDI engine.
    bool init();

    /// Shut down the MIDI engine.
    void shutdown();

    /// Whether the engine is initialized.
    bool is_initialized() const { return initialized_; }

    // ─── MIDI Graph ──────────────────────────────────────────
    MidiGraph& graph() { return graph_; }
    const MidiGraph& graph() const { return graph_; }

    // ─── MIDI Clock ──────────────────────────────────────────
    MidiClock& clock() { return clock_; }
    const MidiClock& clock() const { return clock_; }

    // ─── Note tracking ───────────────────────────────────────
    NoteTracker& note_tracker() { return note_tracker_; }
    const NoteTracker& note_tracker() const { return note_tracker_; }

    // ─── MIDI Drivers ────────────────────────────────────────
    /// Open a platform MIDI device for input.
    bool open_input_device(const MidiDeviceInfo& dev);

    /// Open a platform MIDI device for output.
    bool open_output_device(const MidiDeviceInfo& dev);

    /// Close the current MIDI driver.
    void close_driver();

    /// Get the currently active MIDI driver.
    MidiDriver* driver() const { return driver_.get(); }

    /// Enumerate available MIDI devices.
    std::vector<MidiDeviceInfo> enumerate_devices() const;

    // ─── Recorder ────────────────────────────────────────────
    MidiRecorder& recorder() { return recorder_; }
    const MidiRecorder& recorder() const { return recorder_; }

    /// Start recording MIDI input.
    void start_recording(uint32_t ppqn_position);

    /// Stop recording and finalize the clip.
    void stop_recording();

    /// Whether recording is in progress.
    bool is_recording() const { return recorder_.is_recording(); }

    /// Get the finalized clip from the last recording.
    MidiClip recorded_clip() const { return recorded_clip_; }

    // ─── MPE ─────────────────────────────────────────────────
    MPEDecoder& mpe_decoder() { return mpe_decoder_; }
    const MPEDecoder& mpe_decoder() const { return mpe_decoder_; }

    MPEEncoder& mpe_encoder() { return mpe_encoder_; }
    const MPEEncoder& mpe_encoder() const { return mpe_encoder_; }

    // ─── Process ─────────────────────────────────────────────
    /// Process MIDI for one audio callback.
    /// Updates clock, then processes the MIDI graph.
    void process(uint32_t frames, uint64_t sample_position, double bpm);

    /// Process an incoming MIDI event from a driver.
    /// Routes through MPE decoder, recorder, and note tracker.
    void process_incoming_event(const MidiEvent& event,
                                uint64_t sample_position,
                                double sample_rate);

    // ─── Diagnostics ─────────────────────────────────────────
    /// Number of currently active (sounding) notes.
    uint32_t active_note_count() const {
        return note_tracker_.active_note_count();
    }

private:
    bool initialized_ = false;
    MidiGraph graph_;
    MidiClock clock_;
    NoteTracker note_tracker_;

    // ─── Driver ──────────────────────────────────────────────
    std::unique_ptr<MidiDriver> driver_;

    // ─── Recorder ────────────────────────────────────────────
    MidiRecorder recorder_;
    MidiClip recorded_clip_;

    // ─── MPE ─────────────────────────────────────────────────
    MPEDecoder mpe_decoder_;
    MPEEncoder mpe_encoder_;

    // ─── Internal ────────────────────────────────────────────
    bool driver_callback_attached_ = false;

    /// Driver receive callback — forwards incoming events to
    /// process_incoming_event with audio position.
    void on_driver_event(const MidiEvent& event);
};

} // namespace aria

#endif // ARIA_MIDI_ENGINE_H
