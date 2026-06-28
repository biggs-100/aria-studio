#ifndef ARIA_VENDOR_IVSTPARAMETERCHANGES_H
#define ARIA_VENDOR_IVSTPARAMETERCHANGES_H

#if defined(ARIA_FEATURE_VST3)

#include "../base/funknown.h"

namespace Steinberg {
namespace Vst {

/// A single parameter queue (a group of changes for one param)
class IParamValueQueue : public FUnknown {
public:
    static const FUID iid;

    virtual int32_t getParameterId() = 0;
    virtual int32_t getPointCount() = 0;
    virtual TResult getPoint(int32_t index, int32_t& sample_offset, double& value) = 0;
    virtual TResult addPoint(int32_t sample_offset, double value, int32_t& index) = 0;
};

/// Collection of parameter queues
class IParameterChanges : public FUnknown {
public:
    static const FUID iid;

    virtual int32_t getParameterCount() = 0;
    virtual IParamValueQueue* getParameterData(int32_t index) = 0;
    virtual IParamValueQueue* addParameterData(int32_t param_id, int32_t& index) = 0;
};

} // namespace Vst
} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_IVSTPARAMETERCHANGES_H