#include "virtual_port_driver.h"
#include <chrono>

namespace aria {

// ─── VirtualPortDriver ──────────────────────────────────────────

VirtualPortDriver::VirtualPortDriver() = default;

VirtualPortDriver::~VirtualPortDriver() {
    close();
}

std::vector<MidiDeviceInfo> VirtualPortDriver::enumerate() {
    // Enumerate includes our virtual port plus platform-specific virtuals
    auto devices = enumerate_virtual_ports();

    // Add our own port if open
    if (virtual_port_open_ && !port_name_.empty()) {
        bool found = false;
        for (const auto& d : devices) {
            if (d.name == port_name_) {
                found = true;
                break;
            }
        }
        if (!found) {
            MidiDeviceInfo self;
            self.id        = "aria_virtual_port";
            self.name      = port_name_;
            self.direction = MidiDeviceInfo::Direction::Duplex;
            self.type      = MidiDeviceInfo::Type::Virtual;
            self.is_default = false;
            devices.push_back(std::move(self));
        }
    }

    return devices;
}

bool VirtualPortDriver::open(const MidiDeviceInfo& dev) {
    close();
    port_name_ = dev.name;
    return create_virtual_port(port_name_);
}

void VirtualPortDriver::close() {
    loopback_running_ = false;
    if (loopback_thread_.joinable()) {
        loopback_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::queue<MidiEvent>().swap(loopback_queue_);
    }

    remove_virtual_port();
    virtual_port_open_ = false;
    port_name_.clear();
    clear_error();
}

bool VirtualPortDriver::send(const MidiEvent& event) {
    if (!virtual_port_open_) {
        last_error_ = "Virtual port not open";
        return false;
    }

    // Push to loopback queue for internal routing
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        loopback_queue_.push(event);
    }

    return true;
}

bool VirtualPortDriver::create_virtual_port(const std::string& port_name) {
    if (virtual_port_open_) {
        remove_virtual_port();
    }

    port_name_ = port_name;

#ifdef _WIN32
    // On Windows, create a virtual MIDI port is limited without custom drivers.
    // We use an internal loopback: open a MIDI output to Microsoft GS Wavetable
    // and create a mirror input. For a true virtual port, a separate driver
    // (like loopMIDI) is needed. We document this limitation.
    // For now, the virtual port acts as an internal routing bus.
    virtual_port_open_ = true;
#elif defined(__APPLE__)
    // On macOS, CoreMIDI allows creating external devices virtually.
    // This would use MIDIExternalDeviceCreate — but that requires
    // a MIDI entity/endpoint setup. For a lightweight approach,
    // we use the internal loopback.
    virtual_port_open_ = true;
#else
    // On Linux, we could create an ALSA sequencer port that appears
    // as a virtual MIDI device. We handle this at the factory level
    // via the ALSA driver's port creation.
    virtual_port_open_ = true;
#endif

    // Start loopback polling thread
    if (virtual_port_open_) {
        loopback_running_ = true;
        loopback_thread_ = std::thread(&VirtualPortDriver::loopback_poll, this);
    }

    return true;
}

void VirtualPortDriver::remove_virtual_port() {
#ifdef _WIN32
    // Close internal handles
    if (internal_out_handle_) {
        // midiOutClose(static_cast<HMIDIOUT>(internal_out_handle_));
        internal_out_handle_ = nullptr;
    }
    if (internal_in_handle_) {
        // midiInClose(static_cast<HMIDIIN>(internal_in_handle_));
        internal_in_handle_ = nullptr;
    }
#endif
}

std::vector<MidiDeviceInfo> VirtualPortDriver::enumerate_virtual_ports() const {
    std::vector<MidiDeviceInfo> devices;

    // Add our own port if open
    if (virtual_port_open_ && !port_name_.empty()) {
        MidiDeviceInfo info;
        info.id        = "aria_virtual";
        info.name      = port_name_;
        info.direction = MidiDeviceInfo::Direction::Duplex;
        info.type      = MidiDeviceInfo::Type::Virtual;
        info.is_default = false;
        devices.push_back(info);
    }

    return devices;
}

void VirtualPortDriver::loopback_poll() {
    while (loopback_running_) {
        MidiEvent event;
        bool has_event = false;

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (!loopback_queue_.empty()) {
                event = loopback_queue_.front();
                loopback_queue_.pop();
                has_event = true;
            }
        }

        if (has_event) {
            dispatch_receive(event);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

} // namespace aria
