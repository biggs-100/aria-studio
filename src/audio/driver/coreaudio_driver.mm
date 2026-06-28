#include "coreaudio_driver.h"

#if defined(__APPLE__)

#import <AudioUnit/AudioUnit.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Render Callback Bridge
// ═══════════════════════════════════════════════════════════════

struct CoreAudioRenderBridge {
    /// AudioUnit render callback (called from real-time audio thread).
    static OSStatus render(void* in_ref_con,
                           AudioUnitRenderActionFlags* io_action_flags,
                           const AudioTimeStamp* in_time_stamp,
                           UInt32 in_bus_number,
                           UInt32 in_number_frames,
                           AudioBufferList* io_data)
    {
        auto* self = static_cast<CoreAudioDriver*>(in_ref_con);
        if (!self || !self->callback_) {
            // Silence output if no callback
            if (io_data) {
                for (UInt32 i = 0; i < io_data->mNumberBuffers; ++i) {
                    if (io_data->mBuffers[i].mData) {
                        std::memset(io_data->mBuffers[i].mData, 0,
                                    io_data->mBuffers[i].mDataByteSize);
                    }
                }
            }
            return noErr;
        }

        const uint32_t n_inputs  = self->input_channels_;
        const uint32_t n_outputs = self->output_channels_;
        const uint32_t frames    = static_cast<uint32_t>(in_number_frames);

        // ── Process input bus (bus 1 on RemoteIO) ─────────────
        // RemoteIO: bus 0 = output, bus 1 = input.
        // The input callback runs before the render callback.
        // Input data is available via AudioUnitRender.

        // ── Zero output scratch buffers ────────────────────────
        for (uint32_t ch = 0; ch < n_outputs; ++ch) {
            if (self->output_scratch_ && self->output_scratch_[ch]) {
                std::memset(self->output_scratch_[ch], 0,
                            static_cast<size_t>(frames) * sizeof(float));
            }
        }

        // ── Invoke process callback ────────────────────────────
        float** in_ptr  = (n_inputs > 0 && self->input_scratch_)
                            ? self->input_scratch_ : nullptr;
        float** out_ptr = (n_outputs > 0 && self->output_scratch_)
                            ? self->output_scratch_ : nullptr;

        self->callback_(in_ptr, out_ptr, frames);

        // ── Copy output to AudioUnit buffer list ───────────────
        if (io_data) {
            for (UInt32 ch = 0; ch < io_data->mNumberBuffers; ++ch) {
                if (ch < n_outputs && self->output_scratch_ &&
                    self->output_scratch_[ch] && io_data->mBuffers[ch].mData)
                {
                    std::memcpy(io_data->mBuffers[ch].mData,
                                self->output_scratch_[ch],
                                static_cast<size_t>(frames) * sizeof(float));
                } else if (io_data->mBuffers[ch].mData) {
                    std::memset(io_data->mBuffers[ch].mData, 0,
                                io_data->mBuffers[ch].mDataByteSize);
                }
            }
        }

        return noErr;
    }

    /// Input callback — captures audio from the input bus.
    static OSStatus input_capture(void* in_ref_con,
                                  AudioUnitRenderActionFlags* io_action_flags,
                                  const AudioTimeStamp* in_time_stamp,
                                  UInt32 in_bus_number,
                                  UInt32 in_number_frames,
                                  AudioBufferList* io_data)
    {
        auto* self = static_cast<CoreAudioDriver*>(in_ref_con);
        if (!self || !self->input_scratch_) return noErr;

        // Render input into a local buffer list
        AudioUnit au = static_cast<AudioUnit>(self->audio_unit_);
        if (!au) return noErr;

        const uint32_t n_inputs = self->input_channels_;
        const uint32_t frames   = static_cast<uint32_t>(in_number_frames);

        // Set up buffer list for input capture
        const UInt32 bytes_per_ch = frames * sizeof(float);
        AudioBufferList input_bufs;
        input_bufs.mNumberBuffers = 1;
        input_bufs.mBuffers[0].mNumberChannels = n_inputs;
        input_bufs.mBuffers[0].mDataByteSize   = bytes_per_ch * n_inputs;
        input_bufs.mBuffers[0].mData           = nullptr;

        // We use a temporary interleaved buffer
        // In production, pre-allocate this buffer.
        // For this stub, we use a stack-allocated buffer for small sizes.
        constexpr UInt32 kStackFrames = 4096;
        float stack_buf[kStackFrames * 8];  // max 8 ch * 4096 frames

        float* interleaved = nullptr;
        if (frames * n_inputs <= kStackFrames * 8) {
            interleaved = stack_buf;
        }
        if (!interleaved) return noErr;

        input_bufs.mBuffers[0].mData = interleaved;

        OSStatus err = AudioUnitRender(au, io_action_flags, in_time_stamp,
                                       1, in_number_frames, &input_bufs);
        if (err != noErr) return err;

        // De-interleave into planar scratch buffers
        for (uint32_t ch = 0; ch < n_inputs && ch < frames; ++ch) {
            float* dst = self->input_scratch_[ch];
            if (!dst) continue;
            for (uint32_t f = 0; f < frames; ++f) {
                dst[f] = interleaved[f * n_inputs + ch];
            }
        }

        return noErr;
    }
};

// ═══════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════

CoreAudioDriver::CoreAudioDriver() = default;

CoreAudioDriver::~CoreAudioDriver() {
    close();
}

// ═══════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════

/// Get a string property from an AudioObject.
static std::string get_audio_object_string(AudioObjectID obj_id,
                                            AudioObjectPropertySelector selector,
                                            AudioObjectPropertyScope scope =
                                                kAudioObjectPropertyScopeGlobal)
{
    AudioObjectPropertyAddress addr = {
        selector, scope, kAudioObjectPropertyElementMain
    };

    CFStringRef str = nullptr;
    UInt32 size = sizeof(str);
    OSStatus err = AudioObjectGetPropertyData(
        obj_id, &addr, 0, nullptr, &size, &str);
    if (err != noErr || !str) return "Unknown";

    char buf[512] = {};
    CFStringGetCString(str, buf, sizeof(buf), kCFStringEncodingUTF8);
    CFRelease(str);
    return buf;
}

/// Get the UID of an audio device.
static std::string get_device_uid(AudioDeviceID dev_id) {
    return get_audio_object_string(dev_id, kAudioDevicePropertyDeviceUID);
}

// ═══════════════════════════════════════════════════════════════
// Device Enumeration
// ═══════════════════════════════════════════════════════════════

std::vector<AudioDriver::DeviceInfo> CoreAudioDriver::enumerate_devices() {
    std::vector<DeviceInfo> devices;

    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    UInt32 data_size = 0;
    OSStatus err = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &addr, 0, nullptr, &data_size);
    if (err != noErr || data_size == 0) return devices;

    UInt32 device_count = data_size / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> dev_ids(device_count);

    err = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &addr, 0, nullptr,
        &data_size, dev_ids.data());
    if (err != noErr) return devices;

    // Get default input/output devices
    AudioDeviceID default_output = kAudioObjectUnknown;
    {
        AudioObjectPropertyAddress def_addr = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        UInt32 sz = sizeof(default_output);
        AudioObjectGetPropertyData(
            kAudioObjectSystemObject, &def_addr, 0, nullptr, &sz,
            &default_output);
    }

    AudioDeviceID default_input = kAudioObjectUnknown;
    {
        AudioObjectPropertyAddress def_addr = {
            kAudioHardwarePropertyDefaultInputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        UInt32 sz = sizeof(default_input);
        AudioObjectGetPropertyData(
            kAudioObjectSystemObject, &def_addr, 0, nullptr, &sz,
            &default_input);
    }

    for (AudioDeviceID dev_id : dev_ids) {
        DeviceInfo info;

        // UID
        info.id = get_device_uid(dev_id);

        // Name
        info.name = get_audio_object_string(dev_id,
            kAudioObjectPropertyName);

        // Determine I/O channels
        auto get_channel_count = [&](AudioObjectPropertyScope scope) -> UInt32 {
            AudioObjectPropertyAddress ch_addr = {
                kAudioDevicePropertyStreams,
                scope,
                kAudioObjectPropertyElementMain
            };
            UInt32 sz = 0;
            if (AudioObjectGetPropertyDataSize(dev_id, &ch_addr, 0, nullptr,
                                                &sz) != noErr)
                return 0;
            UInt32 stream_count = sz / sizeof(AudioStreamID);
            if (stream_count == 0) return 0;

            std::vector<AudioStreamID> streams(stream_count);
            sz = stream_count * sizeof(AudioStreamID);
            if (AudioObjectGetPropertyData(dev_id, &ch_addr, 0, nullptr, &sz,
                                            streams.data()) != noErr)
                return 0;

            UInt32 total = 0;
            for (auto sid : streams) {
                AudioObjectPropertyAddress v_addr = {
                    kAudioStreamPropertyVirtualFormat,
                    scope,
                    kAudioObjectPropertyElementMain
                };
                AudioStreamBasicDescription fmt = {};
                UInt32 fsz = sizeof(fmt);
                if (AudioObjectGetPropertyData(sid, &v_addr, 0, nullptr,
                                                &fsz, &fmt) == noErr)
                {
                    total += fmt.mChannelsPerFrame;
                }
            }
            return total;
        };

        info.output_channels = get_channel_count(
            kAudioDevicePropertyScopeOutput);
        info.input_channels  = get_channel_count(
            kAudioDevicePropertyScopeInput);

        // Skip devices with no I/O
        if (info.input_channels == 0 && info.output_channels == 0)
            continue;

        // Gather supported sample rates
        AudioObjectPropertyAddress sr_addr = {
            kAudioDevicePropertyAvailableNominalSampleRates,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        UInt32 sr_size = 0;
        if (AudioObjectGetPropertyDataSize(dev_id, &sr_addr, 0, nullptr,
                                            &sr_size) == noErr)
        {
            UInt32 rate_count = sr_size / sizeof(AudioValueRange);
            std::vector<AudioValueRange> ranges(rate_count);
            sr_size = rate_count * sizeof(AudioValueRange);
            if (AudioObjectGetPropertyData(dev_id, &sr_addr, 0, nullptr,
                                            &sr_size, ranges.data()) == noErr)
            {
                for (const auto& range : ranges) {
                    uint32_t sr = static_cast<uint32_t>(range.mMinimum + 0.5);
                    if (sr > 0) {
                        info.sample_rates.push_back(sr);
                    }
                }
            }
        }

        // If no rates discovered, provide common defaults
        if (info.sample_rates.empty()) {
            info.sample_rates = { 44100, 48000, 96000, 192000 };
        }

        info.buffer_sizes = { 32, 64, 128, 256, 512, 1024 };
        info.is_default =
            (dev_id == default_output || dev_id == default_input);

        devices.push_back(std::move(info));
    }

    return devices;
}

// ═══════════════════════════════════════════════════════════════
// Open / Close
// ═══════════════════════════════════════════════════════════════

bool CoreAudioDriver::open(const DeviceInfo& dev, uint32_t sample_rate,
                            uint32_t buffer_size)
{
    if (open_.load()) close();

    device_uid_     = dev.id;
    input_channels_  = dev.input_channels;
    output_channels_ = dev.output_channels;

    // ── Create AudioComponent for RemoteIO ────────────────────
    AudioComponentDescription desc;
    desc.componentType         = kAudioUnitType_Output;
    desc.componentSubType      = kAudioUnitSubType_RemoteIO;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags        = 0;
    desc.componentFlagsMask    = 0;

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) return false;

    AudioUnit au = nullptr;
    OSStatus err = AudioComponentInstanceNew(comp, &au);
    if (err != noErr || !au) return false;

    // ── Enable I/O on both buses ──────────────────────────────
    // Enable output (bus 0) — enabled by default on RemoteIO
    if (output_channels_ > 0) {
        UInt32 enable = 1;
        err = AudioUnitSetProperty(au,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Output, 0, &enable, sizeof(enable));
        if (err != noErr) {
            AudioComponentInstanceDispose(au);
            return false;
        }
    }

    // Enable input (bus 1) — disabled by default
    if (input_channels_ > 0) {
        UInt32 enable = 1;
        err = AudioUnitSetProperty(au,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Input, 1, &enable, sizeof(enable));
        if (err != noErr) {
            AudioComponentInstanceDispose(au);
            return false;
        }
    }

    // ── Set stream format — float non-interleaved ────────────
    AudioStreamBasicDescription stream_desc = {};
    stream_desc.mFormatID         = kAudioFormatLinearPCM;
    stream_desc.mFormatFlags      = kAudioFormatFlagIsFloat
                                  | kAudioFormatFlagIsPacked
                                  | kAudioFormatFlagIsNonInterleaved;
    stream_desc.mSampleRate       = sample_rate;
    stream_desc.mBitsPerChannel   = 32;
    stream_desc.mFramesPerPacket  = 1;
    stream_desc.mChannelsPerFrame = output_channels_;
    stream_desc.mBytesPerFrame    = sizeof(float);
    stream_desc.mBytesPerPacket   = sizeof(float);

    // Output side (bus 0, scope output)
    err = AudioUnitSetProperty(au,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Output, 0,
        &stream_desc, sizeof(stream_desc));
    if (err != noErr) {
        AudioComponentInstanceDispose(au);
        return false;
    }

    // Input side (bus 1, scope input) — use same format
    if (input_channels_ > 0) {
        stream_desc.mChannelsPerFrame = input_channels_;
        err = AudioUnitSetProperty(au,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input, 1,
            &stream_desc, sizeof(stream_desc));
        if (err != noErr) {
            AudioComponentInstanceDispose(au);
            return false;
        }
    }

    // ── Set buffer size ───────────────────────────────────────
    err = AudioUnitSetProperty(au,
        kAudioUnitProperty_MaximumFramesPerSlice,
        kAudioUnitScope_Global, 0,
        &buffer_size, sizeof(buffer_size));
    if (err != noErr) {
        AudioComponentInstanceDispose(au);
        return false;
    }

    // ── Set device ────────────────────────────────────────────
    // Find the AudioDeviceID from the UID
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 data_size = 0;
    AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &addr, 0, nullptr, &data_size);
    if (data_size > 0) {
        UInt32 dev_count = data_size / sizeof(AudioDeviceID);
        std::vector<AudioDeviceID> dev_ids(dev_count);
        data_size = dev_count * sizeof(AudioDeviceID);
        if (AudioObjectGetPropertyData(
                kAudioObjectSystemObject, &addr, 0, nullptr,
                &data_size, dev_ids.data()) == noErr)
        {
            for (AudioDeviceID aid : dev_ids) {
                if (get_device_uid(aid) == dev.id) {
                    err = AudioUnitSetProperty(au,
                        kAudioOutputUnitProperty_CurrentDevice,
                        kAudioUnitScope_Global, 0,
                        &aid, sizeof(aid));
                    break;
                }
            }
        }
    }

    // ── Allocate scratch buffers ──────────────────────────────
    if (input_channels_ > 0) {
        input_scratch_ = new float*[input_channels_];
        for (uint32_t i = 0; i < input_channels_; ++i) {
            input_scratch_[i] = new float[buffer_size]();
        }
    }
    if (output_channels_ > 0) {
        output_scratch_ = new float*[output_channels_];
        for (uint32_t i = 0; i < output_channels_; ++i) {
            output_scratch_[i] = new float[buffer_size]();
        }
    }

    // ── Set render callback — output ──────────────────────────
    AURenderCallbackStruct output_cb;
    output_cb.inputProc       = CoreAudioRenderBridge::render;
    output_cb.inputProcRefCon = this;

    err = AudioUnitSetProperty(au,
        kAudioUnitProperty_SetRenderCallback,
        kAudioUnitScope_Input, 0, &output_cb, sizeof(output_cb));
    if (err != noErr) {
        AudioComponentInstanceDispose(au);
        return false;
    }

    // ── Set input callback if we have input channels ─────────
    if (input_channels_ > 0) {
        AURenderCallbackStruct input_cb;
        input_cb.inputProc       = CoreAudioRenderBridge::input_capture;
        input_cb.inputProcRefCon = this;

        err = AudioUnitSetProperty(au,
            kAudioOutputUnitProperty_SetInputCallback,
            kAudioUnitScope_Global, 0, &input_cb, sizeof(input_cb));
        if (err != noErr) {
            AudioComponentInstanceDispose(au);
            return false;
        }
    }

    // ── Initialize AudioUnit ──────────────────────────────────
    err = AudioUnitInitialize(au);
    if (err != noErr) {
        AudioComponentInstanceDispose(au);
        return false;
    }

    audio_unit_ = au;
    sample_rate_ = sample_rate;
    buffer_size_ = buffer_size;
    open_.store(true);
    return true;
}

void CoreAudioDriver::close() {
    if (!open_.load()) return;

    stop();

    if (audio_unit_) {
        AudioUnit au = static_cast<AudioUnit>(audio_unit_);
        AudioUnitUninitialize(au);
        AudioComponentInstanceDispose(au);
        audio_unit_ = nullptr;
    }

    // Free scratch buffers
    if (input_scratch_) {
        for (uint32_t i = 0; i < input_channels_; ++i) {
            delete[] input_scratch_[i];
        }
        delete[] input_scratch_;
        input_scratch_ = nullptr;
    }
    if (output_scratch_) {
        for (uint32_t i = 0; i < output_channels_; ++i) {
            delete[] output_scratch_[i];
        }
        delete[] output_scratch_;
        output_scratch_ = nullptr;
    }

    input_channels_  = 0;
    output_channels_ = 0;
    sample_rate_.store(0);
    buffer_size_.store(0);
    open_.store(false);
    running_.store(false);
}

// ═══════════════════════════════════════════════════════════════
// Start / Stop
// ═══════════════════════════════════════════════════════════════

bool CoreAudioDriver::start() {
    if (!open_.load() || running_.load()) return false;
    if (!audio_unit_) return false;

    OSStatus err = AudioOutputUnitStart(static_cast<AudioUnit>(audio_unit_));
    if (err != noErr) return false;

    running_.store(true);
    return true;
}

void CoreAudioDriver::stop() {
    if (!running_.load()) return;

    if (audio_unit_) {
        AudioOutputUnitStop(static_cast<AudioUnit>(audio_unit_));
    }

    running_.store(false);
}

// ═══════════════════════════════════════════════════════════════
// State Accessors
// ═══════════════════════════════════════════════════════════════

uint32_t CoreAudioDriver::current_sample_rate() const {
    return sample_rate_.load();
}

uint32_t CoreAudioDriver::current_buffer_size() const {
    return buffer_size_.load();
}

bool CoreAudioDriver::is_open() const {
    return open_.load();
}

bool CoreAudioDriver::is_running() const {
    return running_.load();
}

} // namespace aria

#endif // defined(__APPLE__)
