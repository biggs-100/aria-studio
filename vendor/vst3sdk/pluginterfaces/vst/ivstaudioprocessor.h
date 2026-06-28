#ifndef ARIA_VENDOR_IVSTAUDIOPROCESSOR_H
#define ARIA_VENDOR_IVSTAUDIOPROCESSOR_H

#if defined(ARIA_FEATURE_VST3)

#include "../base/funknown.h"
#include "ivstcomponent.h"

namespace Steinberg {
namespace Vst {

/// Processing modes
enum ProcessModes : int32_t {
    kOffline = 0,
    kPrefetch = 1,
    kRealtime = 2
};

/// Process setup
struct ProcessSetup {
    int32_t process_mode;
    int32_t symbolic_sample_size;
    int32_t max_audio_buffers;
    double sample_rate;
};

/// Process data
struct ProcessData {
    int32_t process_mode;
    int32_t symbolic_sample_size;
    int32_t num_inputs;
    int32_t num_outputs;
    AudioBusBuffers* inputs;
    AudioBusBuffers* outputs;
    double sample_rate;
    uint64_t continous_sample_pos;
    int32_t num_samples;
    void* input_events;
    void* output_events;
    void* input_parameter_changes;
    void* output_parameter_changes;
    void* context;
};

/// IAudioProcessor interface
class IAudioProcessor : public FUnknown {
public:
    static const FUID iid;

    virtual TResult setBusArrangements(SpeakerArrangement* inputs,
                                       int32_t num_ins,
                                       SpeakerArrangement* outputs,
                                       int32_t num_outs) = 0;
    virtual TResult getBusArrangement(BusDirection dir, int32_t index,
                                      SpeakerArrangement& arr) = 0;
    virtual TResult canProcessSampleSize(int32_t symbolic_sample_size) = 0;
    virtual uint32_t getLatencySamples() = 0;
    virtual TResult setupProcessing(const ProcessSetup& setup) = 0;
    virtual TResult setProcessing(TBool state) = 0;
    virtual TResult process(const ProcessData& data) = 0;
    virtual uint32_t getTailSamples() = 0;
};

} // namespace Vst
} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_IVSTAUDIOPROCESSOR_H