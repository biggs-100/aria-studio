#ifndef ARIA_VENDOR_IVSTCOMPONENT_H
#define ARIA_VENDOR_IVSTCOMPONENT_H

#if defined(ARIA_FEATURE_VST3)

#include "../base/funknown.h"

namespace Steinberg {
namespace Vst {

/// Bus types
enum BusType : int32_t {
    kMainInput = 0,
    kMainOutput = 1,
    kAuxInput = 2,
    kAuxOutput = 3
};

/// Bus directions
enum BusDirection : int32_t {
    kInput = 0,
    kOutput = 1
};

/// Bus media types
enum MediaTypes : int32_t {
    kAudio = 0,
    kEvent = 1
};

/// Bus info
struct BusInfo {
    TChar name[64];
    BusType bus_type;
    int32_t channel_count;
    MediaTypes media_type;
    TBool is_enabled;
    int32_t flags;
};

/// Audio bus buffers used in process call
struct AudioBusBuffers {
    int32_t num_channels;
    int32_t silence_flags;
    void* buffers;  // actually float** or double**
};

/// Speaker arrangement
struct SpeakerArrangement {
    uint64_t speaker_mask;
    uint32_t num_channels;
};

/// IComponent interface
class IComponent : public FUnknown {
public:
    static const FUID iid;

    virtual TResult initialize(FUnknown* context) = 0;
    virtual TResult terminate() = 0;
    virtual TResult getControllerClassId(TUID* class_id) = 0;
    virtual TResult setIoMode(int32_t mode) = 0;
    virtual int32_t getBusCount(MediaTypes type, BusDirection dir) = 0;
    virtual TResult getBusInfo(MediaTypes type, BusDirection dir, int32_t index, BusInfo& info) = 0;
    virtual TResult getRoutingInfo(MediaTypes type, BusDirection dir, int32_t index, void* info) = 0;
    virtual TResult activateBus(MediaTypes type, BusDirection dir, int32_t index, TBool state) = 0;
    virtual TResult setActive(TBool active) = 0;
    virtual TResult setState(FUnknown* state) = 0;
    virtual TResult getState(FUnknown* state) = 0;
};

} // namespace Vst
} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_IVSTCOMPONENT_H