#include "winmm_driver.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace aria {

// ─── WinMM callback (thunk) ─────────────────────────────────────

/// Static callback thunk for midiInOpen. Routes to the driver instance.
static void CALLBACK midi_in_callback(HMIDIIN hMidiIn, UINT wMsg,
                                      DWORD_PTR dwInstance, DWORD_PTR dwParam1,
                                      DWORD_PTR /*dwParam2*/) {
    auto* driver = reinterpret_cast<WinMMDriver*>(dwInstance);
    if (!driver) return;

    if (wMsg == MIM_DATA) {
        // dwParam1 contains the MIDI message packed as:
        //   LOWORD = status | (data1 << 8) | (data2 << 16)
        uint32_t msg = static_cast<uint32_t>(dwParam1);
        uint8_t status = static_cast<uint8_t>(msg & 0xFF);
        uint8_t data1  = static_cast<uint8_t>((msg >> 8) & 0xFF);
        uint8_t data2  = static_cast<uint8_t>((msg >> 16) & 0xFF);

        MidiEvent event = driver->parse_midi_message(status, data1, data2);
        event.timestamp_us = GetTickCount64() * 1000;
        driver->dispatch_receive(event);
    } else if (wMsg == MIM_LONGDATA) {
        // SysEx data — re-add the buffer for continued input
        auto* header = reinterpret_cast<MIDIHDR*>(dwParam1);
        if (header) {
            midiInAddBuffer(hMidiIn, header, sizeof(MIDIHDR));
        }
    }
}

// ─── WinMMDriver ────────────────────────────────────────────────

WinMMDriver::WinMMDriver() = default;

WinMMDriver::~WinMMDriver() {
    close();
}

std::vector<MidiDeviceInfo> WinMMDriver::enumerate() {
    std::vector<MidiDeviceInfo> devices;

    // Input devices
    UINT num_in = midiInGetNumDevs();
    for (UINT i = 0; i < num_in; ++i) {
        MIDIINCAPSW caps{};
        if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            MidiDeviceInfo info;
            info.id = "winmm_in_" + std::to_string(i);
            info.name.resize(wcslen(caps.szPname));
            int len = WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1,
                                          info.name.data(),
                                          static_cast<int>(info.name.size()),
                                          nullptr, nullptr);
            if (len > 0) info.name.resize(len);
            info.direction  = MidiDeviceInfo::Direction::Input;
            info.type       = MidiDeviceInfo::Type::Physical;
            info.is_default = (i == 0);
            devices.push_back(std::move(info));
        }
    }

    // Output devices
    UINT num_out = midiOutGetNumDevs();
    for (UINT i = 0; i < num_out; ++i) {
        MIDIOUTCAPSW caps{};
        if (midiOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            MidiDeviceInfo info;
            info.id = "winmm_out_" + std::to_string(i);
            info.name.resize(wcslen(caps.szPname));
            int len = WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1,
                                          info.name.data(),
                                          static_cast<int>(info.name.size()),
                                          nullptr, nullptr);
            if (len > 0) info.name.resize(len);
            info.direction  = MidiDeviceInfo::Direction::Output;
            info.type       = MidiDeviceInfo::Type::Physical;
            info.is_default = (i == 0);
            devices.push_back(std::move(info));
        }
    }

    return devices;
}

bool WinMMDriver::open(const MidiDeviceInfo& dev) {
    close();

    // Parse device index from ID
    size_t sep = dev.id.rfind('_');
    if (sep == std::string::npos) {
        last_error_ = "Invalid device ID: " + dev.id;
        return false;
    }

    int dev_index = 0;
    try {
        dev_index = std::stoi(dev.id.substr(sep + 1));
    } catch (...) {
        last_error_ = "Cannot parse device index from: " + dev.id;
        return false;
    }

    bool need_input  = (dev.direction == MidiDeviceInfo::Direction::Input ||
                        dev.direction == MidiDeviceInfo::Direction::Duplex);
    bool need_output = (dev.direction == MidiDeviceInfo::Direction::Output ||
                        dev.direction == MidiDeviceInfo::Direction::Duplex);

    if (need_input) {
        // Open MIDI input
        MMRESULT res = midiInOpen(reinterpret_cast<LPHMIDIIN>(&input_handle_),
                                  dev_index,
                                  reinterpret_cast<DWORD_PTR>(midi_in_callback),
                                  reinterpret_cast<DWORD_PTR>(this),
                                  CALLBACK_FUNCTION);
        if (res != MMSYSERR_NOERROR) {
            last_error_ = "midiInOpen failed with error " + std::to_string(res);
            close();
            return false;
        }

        // Prepare input buffer
        std::memset(&input_header_, 0, sizeof(input_header_));
        input_header_.lpData         = reinterpret_cast<LPSTR>(input_buffer_);
        input_header_.dwBufferLength = kBufferSize;

        res = midiInPrepareHeader(reinterpret_cast<HMIDIIN>(input_handle_),
                                  reinterpret_cast<LPMIDIHDR>(&input_header_),
                                  sizeof(input_header_));
        if (res != MMSYSERR_NOERROR) {
            last_error_ = "midiInPrepareHeader failed: " + std::to_string(res);
            close();
            return false;
        }

        res = midiInAddBuffer(reinterpret_cast<HMIDIIN>(input_handle_),
                              reinterpret_cast<LPMIDIHDR>(&input_header_),
                              sizeof(input_header_));
        if (res != MMSYSERR_NOERROR) {
            last_error_ = "midiInAddBuffer failed: " + std::to_string(res);
            close();
            return false;
        }

        res = midiInStart(reinterpret_cast<HMIDIIN>(input_handle_));
        if (res != MMSYSERR_NOERROR) {
            last_error_ = "midiInStart failed: " + std::to_string(res);
            close();
            return false;
        }
        input_open_ = true;
    }

    if (need_output) {
        // Open MIDI output
        MMRESULT res = midiOutOpen(reinterpret_cast<LPHMIDIOUT>(&output_handle_),
                                   dev_index, 0, 0, CALLBACK_NULL);
        if (res != MMSYSERR_NOERROR) {
            last_error_ = "midiOutOpen failed: " + std::to_string(res);
            close();
            return false;
        }
        output_open_ = true;
    }

    return true;
}

void WinMMDriver::close() {
    if (input_handle_) {
        auto* hIn = reinterpret_cast<HMIDIIN>(input_handle_);
        midiInStop(hIn);
        midiInReset(hIn);
        midiInUnprepareHeader(hIn, reinterpret_cast<LPMIDIHDR>(&input_header_),
                              sizeof(input_header_));
        midiInClose(hIn);
        input_handle_ = nullptr;
    }
    input_open_ = false;

    if (output_handle_) {
        auto* hOut = reinterpret_cast<HMIDIOUT>(output_handle_);
        midiOutReset(hOut);
        midiOutClose(hOut);
        output_handle_ = nullptr;
    }
    output_open_ = false;

    clear_error();
}

bool WinMMDriver::send(const MidiEvent& event) {
    if (!output_handle_) {
        last_error_ = "No output device open";
        return false;
    }

    // Pack the MIDI message into a 32-bit value:
    //   byte 0: status (with channel)
    //   byte 1: data1
    //   byte 2: data2
    uint32_t msg = static_cast<uint32_t>(event.data2) << 16 |
                   static_cast<uint32_t>(event.data1) << 8  |
                   static_cast<uint32_t>(event.type) | event.channel;

    MMRESULT res = midiOutShortMsg(reinterpret_cast<HMIDIOUT>(output_handle_), msg);
    if (res != MMSYSERR_NOERROR) {
        last_error_ = "midiOutShortMsg failed: " + std::to_string(res);
        return false;
    }
    return true;
}

MidiEvent WinMMDriver::parse_midi_message(uint8_t status, uint8_t data1,
                                          uint8_t data2) {
    MidiEvent event{};
    event.channel = status & 0x0F;
    event.data1   = data1;
    event.data2   = data2;

    switch (status & 0xF0) {
        case 0x80: event.type = MidiMessageType::NoteOff;           break;
        case 0x90: event.type = MidiMessageType::NoteOn;            break;
        case 0xA0: event.type = MidiMessageType::PolyphonicKey;     break;
        case 0xB0: event.type = MidiMessageType::ControlChange;     break;
        case 0xC0: event.type = MidiMessageType::ProgramChange;     break;
        case 0xD0: event.type = MidiMessageType::ChannelAftertouch; break;
        case 0xE0: event.type = MidiMessageType::PitchBend;         break;
        default:
            event.type    = MidiMessageType::NoteOff;
            event.channel = 0;
            break;
    }

    return event;
}

} // namespace aria

#endif // _WIN32
