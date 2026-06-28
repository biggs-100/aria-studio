#include "coremidi_driver.h"

#ifdef __APPLE__

#import <CoreMIDI/CoreMIDI.h>
#import <CoreFoundation/CoreFoundation.h>
#include <algorithm>
#include <sstream>
#include <string>

namespace aria {

// ─── CoreMIDI helpers ───────────────────────────────────────────

/// Extract a std::string from a CFStringRef.
static std::string cfstring_to_string(CFStringRef str) {
    if (!str) return {};
    CFIndex len = CFStringGetLength(str);
    CFIndex max_size = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    std::string result;
    result.resize(static_cast<size_t>(max_size));
    if (CFStringGetCString(str, result.data(), max_size, kCFStringEncodingUTF8)) {
        result.resize(std::strlen(result.data()));
    }
    return result;
}

/// Get the display name of a CoreMIDI endpoint.
static std::string get_endpoint_name(MIDIEndpointRef endpoint) {
    CFStringRef name = nullptr;
    MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
    std::string result = cfstring_to_string(name);
    if (name) CFRelease(name);

    if (result.empty()) {
        MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name);
        result = cfstring_to_string(name);
        if (name) CFRelease(name);
    }
    return result;
}

/// Get the unique ID of a CoreMIDI endpoint.
static int32_t get_endpoint_unique_id(MIDIEndpointRef endpoint) {
    SInt32 id = 0;
    MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &id);
    return static_cast<int32_t>(id);
}

// ─── CoreMIDI callback (static) ─────────────────────────────────

/// Receive callback for MIDI input. Called from CoreMIDI's thread.
static void midi_read_proc(const MIDIPacketList* pktlist, void* refCon,
                           void* /*srcConnRefCon*/) {
    auto* driver = static_cast<CoreMidiDriver*>(refCon);
    if (driver) {
        driver->process_packet_list(pktlist);
    }
}

// ─── CoreMidiDriver ─────────────────────────────────────────────

CoreMidiDriver::CoreMidiDriver() = default;

CoreMidiDriver::~CoreMidiDriver() {
    close();
}

std::vector<MidiDeviceInfo> CoreMidiDriver::enumerate() {
    std::vector<MidiDeviceInfo> devices;

    // Input sources
    ItemCount src_count = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < src_count; ++i) {
        MIDIEndpointRef src = MIDIGetSource(i);
        if (!src) continue;

        MidiDeviceInfo info;
        info.id        = "coremidi_src_" + std::to_string(get_endpoint_unique_id(src));
        info.name      = get_endpoint_name(src);
        info.direction = MidiDeviceInfo::Direction::Input;
        info.type      = MidiDeviceInfo::Type::Physical;
        info.is_default = (i == 0);
        devices.push_back(std::move(info));
    }

    // Output destinations
    ItemCount dst_count = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < dst_count; ++i) {
        MIDIEndpointRef dst = MIDIGetDestination(i);
        if (!dst) continue;

        MidiDeviceInfo info;
        info.id        = "coremidi_dst_" + std::to_string(get_endpoint_unique_id(dst));
        info.name      = get_endpoint_name(dst);
        info.direction = MidiDeviceInfo::Direction::Output;
        info.type      = MidiDeviceInfo::Type::Physical;
        info.is_default = (i == 0);
        devices.push_back(std::move(info));
    }

    return devices;
}

bool CoreMidiDriver::open(const MidiDeviceInfo& dev) {
    close();

    // Create MIDI client
    CFStringRef client_name = CFStringCreateWithCString(nullptr, "ARIA DAW",
                                                        kCFStringEncodingUTF8);
    MIDIClientRef client = nullptr;
    OSStatus status = MIDIClientCreate(client_name, nullptr, nullptr, &client);
    CFRelease(client_name);

    if (status != noErr) {
        last_error_ = "MIDIClientCreate failed: " + std::to_string(status);
        return false;
    }
    client_ = client;

    // Determine endpoint and create appropriate port
    size_t sep = dev.id.rfind('_');
    if (sep == std::string::npos) {
        last_error_ = "Invalid device ID: " + dev.id;
        close();
        return false;
    }

    bool is_source = dev.id.find("src_") != std::string::npos;
    bool need_input  = (dev.direction == MidiDeviceInfo::Direction::Input ||
                        dev.direction == MidiDeviceInfo::Direction::Duplex);
    bool need_output = (dev.direction == MidiDeviceInfo::Direction::Output ||
                        dev.direction == MidiDeviceInfo::Direction::Duplex);

    if (need_input && is_source) {
        CFStringRef port_name = CFStringCreateWithCString(nullptr, "ARIA MIDI Input",
                                                          kCFStringEncodingUTF8);
        MIDIPortRef inPort = nullptr;
        status = MIDIInputPortCreate(client, port_name, midi_read_proc,
                                     this, &inPort);
        CFRelease(port_name);

        if (status != noErr) {
            last_error_ = "MIDIInputPortCreate failed: " + std::to_string(status);
            close();
            return false;
        }
        input_port_ = inPort;

        // Connect to source
        // Find endpoint by unique ID
        int32_t target_id = 0;
        try {
            target_id = std::stoi(dev.id.substr(sep + 1));
        } catch (...) {
            last_error_ = "Cannot parse endpoint ID";
            close();
            return false;
        }

        ItemCount src_count = MIDIGetNumberOfSources();
        for (ItemCount i = 0; i < src_count; ++i) {
            MIDIEndpointRef src = MIDIGetSource(i);
            if (src && get_endpoint_unique_id(src) == target_id) {
                MIDIPortConnectSource(static_cast<MIDIPortRef>(input_port_),
                                      src, nullptr);
                input_endpoint_ = static_cast<int>(i);
                break;
            }
        }
    }

    if (need_output && !is_source) {
        CFStringRef port_name = CFStringCreateWithCString(nullptr, "ARIA MIDI Output",
                                                          kCFStringEncodingUTF8);
        MIDIPortRef outPort = nullptr;
        status = MIDIOutputPortCreate(client, port_name, &outPort);
        CFRelease(port_name);

        if (status != noErr) {
            last_error_ = "MIDIOutputPortCreate failed: " + std::to_string(status);
            close();
            return false;
        }
        output_port_ = outPort;

        // Resolve destination
        int32_t target_id = 0;
        try {
            target_id = std::stoi(dev.id.substr(sep + 1));
        } catch (...) {
            last_error_ = "Cannot parse endpoint ID";
            close();
            return false;
        }

        ItemCount dst_count = MIDIGetNumberOfDestinations();
        for (ItemCount i = 0; i < dst_count; ++i) {
            MIDIEndpointRef dst = MIDIGetDestination(i);
            if (dst && get_endpoint_unique_id(dst) == target_id) {
                output_endpoint_ = static_cast<int>(i);
                break;
            }
        }
    }

    return true;
}

void CoreMidiDriver::close() {
    if (input_port_) {
        MIDIPortDispose(static_cast<MIDIPortRef>(input_port_));
        input_port_ = nullptr;
    }
    if (output_port_) {
        MIDIPortDispose(static_cast<MIDIPortRef>(output_port_));
        output_port_ = nullptr;
    }
    if (client_) {
        MIDIClientDispose(static_cast<MIDIClientRef>(client_));
        client_ = nullptr;
    }
    input_endpoint_ = -1;
    output_endpoint_ = -1;
    clear_error();
}

bool CoreMidiDriver::send(const MidiEvent& event) {
    if (!output_port_ || output_endpoint_ < 0) {
        last_error_ = "No output endpoint open";
        return false;
    }

    MIDIEndpointRef dst = MIDIGetDestination(static_cast<ItemCount>(output_endpoint_));
    if (!dst) {
        last_error_ = "Output endpoint no longer available";
        return false;
    }

    // Build a MIDIPacketList with one packet
    ByteCount packet_size = 3;  // status + data1 + data2 (most messages)
    if (event.type == MidiMessageType::ProgramChange ||
        event.type == MidiMessageType::ChannelAftertouch) {
        packet_size = 2;  // 2-byte messages
    }

    Byte buffer[sizeof(MIDIPacketList) + 4] = {};
    MIDIPacketList* pktlist = reinterpret_cast<MIDIPacketList*>(buffer);
    MIDIPacket* packet = MIDIPacketListInit(pktlist);

    Byte data[3] = {};
    data[0] = static_cast<Byte>(static_cast<uint8_t>(event.type) | event.channel);
    data[1] = event.data1;
    data[2] = event.data2;

    packet = MIDIPacketListAdd(pktlist, sizeof(buffer), packet, 0,
                               packet_size, data);
    if (!packet) {
        last_error_ = "MIDIPacketListAdd failed";
        return false;
    }

    OSStatus status = MIDISend(static_cast<MIDIPortRef>(output_port_), dst, pktlist);
    if (status != noErr) {
        last_error_ = "MIDISend failed: " + std::to_string(status);
        return false;
    }
    return true;
}

void CoreMidiDriver::process_packet_list(const void* pktlist) {
    const auto* packets = static_cast<const MIDIPacketList*>(pktlist);
    const MIDIPacket* packet = &packets->packet[0];

    for (UInt32 i = 0; i < packets->numPackets; ++i) {
        const Byte* data = packet->data;
        UInt16 length = packet->length;

        if (length >= 1) {
            UInt16 pos = 0;
            while (pos < length) {
                uint8_t status = data[pos];
                if (status < 0x80) {
                    // Running status — skip
                    ++pos;
                    continue;
                }
                ++pos;

                MidiEvent event{};
                event.channel = status & 0x0F;
                event.timestamp_us = static_cast<uint64_t>(
                    mach_absolute_time() / 1000); // approximate

                uint8_t msg_type = status & 0xF0;
                switch (msg_type) {
                    case 0x80:
                        event.type = MidiMessageType::NoteOff;
                        event.data1 = (pos < length) ? data[pos++] : 0;
                        event.data2 = (pos < length) ? data[pos++] : 0;
                        break;
                    case 0x90:
                        event.type = MidiMessageType::NoteOn;
                        event.data1 = (pos < length) ? data[pos++] : 0;
                        event.data2 = (pos < length) ? data[pos++] : 0;
                        break;
                    case 0xA0:
                        event.type = MidiMessageType::PolyphonicKey;
                        event.data1 = (pos < length) ? data[pos++] : 0;
                        event.data2 = (pos < length) ? data[pos++] : 0;
                        break;
                    case 0xB0:
                        event.type = MidiMessageType::ControlChange;
                        event.data1 = (pos < length) ? data[pos++] : 0;
                        event.data2 = (pos < length) ? data[pos++] : 0;
                        break;
                    case 0xC0:
                        event.type = MidiMessageType::ProgramChange;
                        event.data1 = (pos < length) ? data[pos++] : 0;
                        event.data2 = 0;
                        break;
                    case 0xD0:
                        event.type = MidiMessageType::ChannelAftertouch;
                        event.data1 = (pos < length) ? data[pos++] : 0;
                        event.data2 = 0;
                        break;
                    case 0xE0:
                        event.type = MidiMessageType::PitchBend;
                        event.data1 = (pos < length) ? data[pos++] : 0;
                        event.data2 = (pos < length) ? data[pos++] : 0;
                        break;
                    default:
                        // System message — skip
                        if (status <= 0xF7) {
                            // variable length, just skip rest
                            pos = length;
                        } else if (status == 0xF0) {
                            // SysEx start — skip to 0xF7
                            while (pos < length && data[pos] != 0xF7) ++pos;
                            if (pos < length) ++pos; // skip 0xF7
                        } else {
                            // Real-time — single byte
                        }
                        break;
                }

                if (event.type != MidiMessageType::NoteOff ||
                    event.data1 != 0 || event.data2 != 0) {
                    dispatch_receive(event);
                }
            }
        }

        packet = MIDIPacketNext(packet);
    }
}

} // namespace aria

#endif // __APPLE__
