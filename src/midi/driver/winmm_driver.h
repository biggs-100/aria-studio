#ifndef ARIA_WINMM_DRIVER_H
#define ARIA_WINMM_DRIVER_H

#include "midi_driver.h"

#ifdef _WIN32

namespace aria {

/// Windows Multimedia MIDI driver.
///
/// Uses midiInOpen/midiInAddBuffer for input and midiOutShortMsg
/// for output. Enumerates via midiInGetNumDevs / midiOutGetNumDevs.
/// Callback-based — the system calls back on a separate thread for
/// incoming MIDI data.
class WinMMDriver : public MidiDriver {
public:
    WinMMDriver();
    ~WinMMDriver() override;

    WinMMDriver(const WinMMDriver&) = delete;
    WinMMDriver& operator=(const WinMMDriver&) = delete;

    // ─── MidiDriver interface ──────────────────────────────
    std::vector<MidiDeviceInfo> enumerate() override;
    bool open(const MidiDeviceInfo& dev) override;
    void close() override;
    bool send(const MidiEvent& event) override;
    bool is_open() const override { return input_handle_ != nullptr || output_handle_ != nullptr; }

    // Internal: parse raw MIDI bytes into MidiEvent.
    // Public static so the WinMM callback thunk can use it.
    static MidiEvent parse_midi_message(uint8_t status, uint8_t data1, uint8_t data2);

private:
    // Platform handles are opaque pointers (HMIDIIN / HMIDIOUT).
    void* input_handle_  = nullptr;
    void* output_handle_ = nullptr;

    // System MIDI header for input buffering.
    struct midi_header_t {
        void* lpData         = nullptr;
        uint32_t dwBufferLength = 0;
        uint32_t dwBytesRecorded = 0;
        uint32_t dwUser      = 0;
        uint32_t dwFlags     = 0;
        midi_header_t* lpNext = nullptr;
        uint32_t reserved    = 0;
        uint32_t dwOffset    = 0;
        void* dwReserved[8]  = {};
    };

    static constexpr size_t kBufferSize = 1024;
    uint8_t                 input_buffer_[kBufferSize]{};
    midi_header_t           input_header_{};

    // Open mode flags
    bool input_open_  = false;
    bool output_open_ = false;
};

} // namespace aria

#endif // _WIN32

#endif // ARIA_WINMM_DRIVER_H
