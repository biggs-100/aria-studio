#include "alsa_seq_driver.h"

#ifdef __linux__

#include <alsa/asoundlib.h>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace aria {

// ─── AlsaSeqDriver ──────────────────────────────────────────────

AlsaSeqDriver::AlsaSeqDriver() = default;

AlsaSeqDriver::~AlsaSeqDriver() {
    close();
}

std::vector<MidiDeviceInfo> AlsaSeqDriver::enumerate() {
    std::vector<MidiDeviceInfo> devices;

    // Open a temporary sequencer to query subscribers
    snd_seq_t* temp_seq = nullptr;
    if (snd_seq_open(&temp_seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        return devices;
    }

    // List all clients
    snd_seq_client_info_t* client_info = nullptr;
    snd_seq_client_info_alloca(&client_info);
    snd_seq_client_info_set_client(client_info, -1);

    while (snd_seq_query_next_client(temp_seq, client_info) >= 0) {
        int client_id = snd_seq_client_info_get_client(client_info);
        if (client_id == SND_SEQ_CLIENT_SYSTEM) continue;

        // List ports for this client
        snd_seq_port_info_t* port_info = nullptr;
        snd_seq_port_info_alloca(&port_info);
        snd_seq_port_info_set_client(port_info, client_id);
        snd_seq_port_info_set_port(port_info, -1);

        while (snd_seq_query_next_port(temp_seq, port_info) >= 0) {
            unsigned int cap = snd_seq_port_info_get_capability(port_info);

            bool is_input  = (cap & SND_SEQ_PORT_CAP_READ) != 0;
            bool is_output = (cap & SND_SEQ_PORT_CAP_WRITE) != 0;

            if (!is_input && !is_output) continue;

            MidiDeviceInfo info;
            info.id   = "alsa_" + std::to_string(client_id) + "_" +
                         std::to_string(snd_seq_port_info_get_port(port_info));
            info.name = snd_seq_port_info_get_name(port_info);

            if (is_input && is_output) {
                info.direction = MidiDeviceInfo::Direction::Duplex;
            } else if (is_input) {
                info.direction = MidiDeviceInfo::Direction::Input;
            } else {
                info.direction = MidiDeviceInfo::Direction::Output;
            }

            unsigned int port_type = snd_seq_port_info_get_type(port_info);
            if (port_type & SND_SEQ_PORT_TYPE_MIDI_GENERIC ||
                port_type & SND_SEQ_PORT_TYPE_HARDWARE) {
                info.type = MidiDeviceInfo::Type::Physical;
            } else {
                info.type = MidiDeviceInfo::Type::Virtual;
            }

            info.is_default = devices.empty();
            devices.push_back(std::move(info));
        }
    }

    snd_seq_close(temp_seq);
    return devices;
}

bool AlsaSeqDriver::open(const MidiDeviceInfo& dev) {
    close();

    // Open ALSA sequencer
    snd_seq_t* seq = nullptr;
    int err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
    if (err < 0) {
        last_error_ = "snd_seq_open failed: " + std::string(snd_strerror(err));
        return false;
    }
    seq_handle_ = seq;

    // Set client name
    snd_seq_set_client_name(seq, "ARIA DAW");

    // Create a port
    int port = snd_seq_create_simple_port(
        seq, "MIDI I/O",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE |
        SND_SEQ_PORT_CAP_SUBS_READ | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION);
    if (port < 0) {
        last_error_ = "snd_seq_create_simple_port failed: " +
                      std::string(snd_strerror(port));
        close();
        return false;
    }
    port_id_ = port;
    client_id_ = snd_seq_client_id(seq);

    // Parse device ID to find destination
    // Format: "alsa_client_port"
    size_t sep1 = dev.id.rfind('_');
    if (sep1 == std::string::npos) {
        last_error_ = "Invalid device ID: " + dev.id;
        close();
        return false;
    }
    size_t sep2 = dev.id.rfind('_', sep1 - 1);
    if (sep2 == std::string::npos) {
        last_error_ = "Cannot parse client:port from: " + dev.id;
        close();
        return false;
    }

    try {
        dest_client_ = std::stoi(dev.id.substr(sep2 + 1, sep1 - sep2 - 1));
        dest_port_   = std::stoi(dev.id.substr(sep1 + 1));
    } catch (...) {
        last_error_ = "Cannot parse client/port numbers";
        close();
        return false;
    }

    // Subscribe to the source if input is needed
    bool need_input = (dev.direction == MidiDeviceInfo::Direction::Input ||
                       dev.direction == MidiDeviceInfo::Direction::Duplex);
    bool need_output = (dev.direction == MidiDeviceInfo::Direction::Output ||
                        dev.direction == MidiDeviceInfo::Direction::Duplex);

    if (need_input) {
        // Subscribe: connect the device's output to our input port
        snd_seq_port_subscribe_t* subs = nullptr;
        snd_seq_port_subscribe_alloca(&subs);
        snd_seq_port_subscribe_set_sender(subs,
            snd_seq_address(static_cast<unsigned char>(dest_client_),
                            static_cast<unsigned char>(dest_port_)));
        snd_seq_port_subscribe_set_dest(subs,
            snd_seq_address(static_cast<unsigned char>(client_id_),
                            static_cast<unsigned char>(port_id_)));

        err = snd_seq_subscribe_port(seq, subs);
        if (err < 0) {
            last_error_ = "snd_seq_subscribe_port (input) failed: " +
                          std::string(snd_strerror(err));
            close();
            return false;
        }
    }

    // Subscribe to the destination if output is needed
    if (need_output) {
        snd_seq_port_subscribe_t* subs = nullptr;
        snd_seq_port_subscribe_alloca(&subs);
        snd_seq_port_subscribe_set_sender(subs,
            snd_seq_address(static_cast<unsigned char>(client_id_),
                            static_cast<unsigned char>(port_id_)));
        snd_seq_port_subscribe_set_dest(subs,
            snd_seq_address(static_cast<unsigned char>(dest_client_),
                            static_cast<unsigned char>(dest_port_)));

        err = snd_seq_subscribe_port(seq, subs);
        if (err < 0) {
            last_error_ = "snd_seq_subscribe_port (output) failed: " +
                          std::string(snd_strerror(err));
            close();
            return false;
        }
    }

    // Start input polling thread
    if (need_input) {
        running_ = true;
        poll_thread_ = std::thread(&AlsaSeqDriver::poll_loop, this);
    }

    return true;
}

void AlsaSeqDriver::close() {
    running_ = false;
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }

    if (seq_handle_) {
        snd_seq_close(static_cast<snd_seq_t*>(seq_handle_));
        seq_handle_ = nullptr;
    }

    client_id_ = -1;
    port_id_   = -1;
    dest_client_ = -1;
    dest_port_   = -1;
    clear_error();
}

bool AlsaSeqDriver::send(const MidiEvent& event) {
    if (!seq_handle_) {
        last_error_ = "Sequencer not open";
        return false;
    }

    snd_seq_t* seq = static_cast<snd_seq_t*>(seq_handle_);

    // Allocate and populate the ALSA event
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, static_cast<unsigned char>(port_id_));
    snd_seq_ev_set_dest(&ev, static_cast<unsigned char>(dest_client_),
                               static_cast<unsigned char>(dest_port_));
    snd_seq_ev_set_direct(&ev);

    uint8_t channel = event.channel;

    switch (event.type) {
        case MidiMessageType::NoteOff:
            snd_seq_ev_set_noteoff(&ev, channel, event.data1, event.data2);
            break;
        case MidiMessageType::NoteOn:
            snd_seq_ev_set_noteon(&ev, channel, event.data1, event.data2);
            break;
        case MidiMessageType::PolyphonicKey:
            snd_seq_ev_set_keypress(&ev, channel, event.data1, event.data2);
            break;
        case MidiMessageType::ControlChange:
            snd_seq_ev_set_controller(&ev, channel, event.data1, event.data2);
            break;
        case MidiMessageType::ProgramChange:
            snd_seq_ev_set_pgmchange(&ev, channel, event.data1);
            break;
        case MidiMessageType::ChannelAftertouch:
            snd_seq_ev_set_chanpress(&ev, channel, event.data1);
            break;
        case MidiMessageType::PitchBend:
            snd_seq_ev_set_pitchbend(&ev, channel,
                static_cast<int>(event.data1) |
                (static_cast<int>(event.data2) << 7));
            break;
        default:
            return true; // Unsupported message type
    }

    int err = snd_seq_event_output(seq, &ev);
    if (err < 0) {
        last_error_ = "snd_seq_event_output failed: " +
                      std::string(snd_strerror(err));
        return false;
    }

    snd_seq_drain_output(seq);
    return true;
}

void AlsaSeqDriver::poll_loop() {
    snd_seq_t* seq = static_cast<snd_seq_t*>(seq_handle_);
    if (!seq) return;

    int npfd = snd_seq_poll_descriptors_count(seq, POLLIN);
    struct pollfd* pfd = static_cast<struct pollfd*>(
        std::malloc(static_cast<size_t>(npfd) * sizeof(struct pollfd)));
    if (!pfd) return;

    snd_seq_poll_descriptors(seq, pfd, static_cast<unsigned int>(npfd), POLLIN);

    while (running_) {
        int err = poll(pfd, static_cast<nfds_t>(npfd), 10); // 10ms timeout
        if (err < 0) break;
        if (err == 0) continue; // timeout

        // Drain all available events
        while (running_) {
            snd_seq_event_t* alsa_ev = nullptr;
            err = snd_seq_event_input(seq, &alsa_ev);
            if (err < 0 || alsa_ev == nullptr) break;

            MidiEvent event = alsa_event_to_midi(alsa_ev);
            dispatch_receive(event);
            snd_seq_free_event(alsa_ev);
        }
    }

    std::free(pfd);
}

MidiEvent AlsaSeqDriver::alsa_event_to_midi(const void* alsa_ev) const {
    const auto* ev = static_cast<const snd_seq_event_t*>(alsa_ev);
    MidiEvent event{};
    event.channel = ev->data.note.channel;
    event.timestamp_us = static_cast<uint64_t>(ev->time.tick); // approximate

    switch (ev->type) {
        case SND_SEQ_EVENT_NOTEON:
            event.type = MidiMessageType::NoteOn;
            event.data1 = ev->data.note.note;
            event.data2 = ev->data.note.velocity;
            break;
        case SND_SEQ_EVENT_NOTEOFF:
            event.type = MidiMessageType::NoteOff;
            event.data1 = ev->data.note.note;
            event.data2 = ev->data.note.velocity;
            break;
        case SND_SEQ_EVENT_KEYPRESS:
            event.type = MidiMessageType::PolyphonicKey;
            event.data1 = ev->data.note.note;
            event.data2 = ev->data.note.velocity;
            break;
        case SND_SEQ_EVENT_CONTROLLER:
            event.type = MidiMessageType::ControlChange;
            event.data1 = ev->data.control.param;
            event.data2 = ev->data.control.value;
            break;
        case SND_SEQ_EVENT_PGMCHANGE:
            event.type = MidiMessageType::ProgramChange;
            event.data1 = ev->data.control.value;
            break;
        case SND_SEQ_EVENT_CHANPRESS:
            event.type = MidiMessageType::ChannelAftertouch;
            event.data1 = ev->data.control.value;
            break;
        case SND_SEQ_EVENT_PITCHBEND:
            event.type = MidiMessageType::PitchBend;
            event.data1 = static_cast<uint8_t>(ev->data.control.value & 0x7F);
            event.data2 = static_cast<uint8_t>((ev->data.control.value >> 7) & 0x7F);
            break;
        default:
            break;
    }

    return event;
}

} // namespace aria

#endif // __linux__
