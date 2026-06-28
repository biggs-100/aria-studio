#ifndef ARIA_ALSA_SEQ_DRIVER_H
#define ARIA_ALSA_SEQ_DRIVER_H

#include "midi_driver.h"
#include <thread>
#include <atomic>

#ifdef __linux__

namespace aria {

/// Linux ALSA Sequencer MIDI driver.
///
/// Uses the ALSA sequencer interface (snd_seq_open, snd_seq_event_input/output).
/// Runs a background polling thread for event input. Supports duplex operation.
class AlsaSeqDriver : public MidiDriver {
public:
    AlsaSeqDriver();
    ~AlsaSeqDriver() override;

    AlsaSeqDriver(const AlsaSeqDriver&) = delete;
    AlsaSeqDriver& operator=(const AlsaSeqDriver&) = delete;

    // ─── MidiDriver interface ──────────────────────────────
    std::vector<MidiDeviceInfo> enumerate() override;
    bool open(const MidiDeviceInfo& dev) override;
    void close() override;
    bool send(const MidiEvent& event) override;
    bool is_open() const override { return seq_handle_ != nullptr; }

private:
    // Opaque snd_seq_t handle
    void* seq_handle_ = nullptr;

    // Local sequencer client/port
    int client_id_ = -1;
    int port_id_   = -1;

    // Connected device port
    int dest_client_ = -1;
    int dest_port_   = -1;

    // Input polling thread
    std::thread poll_thread_;
    std::atomic<bool> running_{false};

    // Polling loop for incoming MIDI events.
    void poll_loop();

    // Convert snd_seq_event_t to MidiEvent.
    MidiEvent alsa_event_to_midi(const void* alsa_ev) const;
};

} // namespace aria

#endif // __linux__

#endif // ARIA_ALSA_SEQ_DRIVER_H
