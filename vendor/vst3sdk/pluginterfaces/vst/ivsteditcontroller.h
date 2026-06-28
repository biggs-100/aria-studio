#ifndef ARIA_VENDOR_IVSTEDITCONTROLLER_H
#define ARIA_VENDOR_IVSTEDITCONTROLLER_H

#if defined(ARIA_FEATURE_VST3)

#include "../base/funknown.h"

namespace Steinberg {
namespace Vst {

/// Parameter info
struct ParameterInfo {
    FUID id;
    TChar title[64];
    TChar short_title[32];
    TChar units[64];
    int32_t step_count;
    double default_normalized_value;
    int32_t unit_id;
    int32_t flags;
};

/// IEditController interface
class IEditController : public FUnknown {
public:
    static const FUID iid;

    virtual TResult initialize(FUnknown* context) = 0;
    virtual TResult terminate() = 0;
    virtual TResult setComponentState(void* state) = 0;
    virtual TResult setState(void* state) = 0;
    virtual TResult getState(void* state) = 0;
    virtual int32_t getParameterCount() = 0;
    virtual TResult getParameterInfo(int32_t param_index, ParameterInfo& info) = 0;
    virtual TResult getParamStringByValue(int32_t param_id, double value, TChar* string) = 0;
    virtual TResult getParamValueByString(int32_t param_id, const TChar* string, double& value) = 0;
    virtual double normalizedParamToPlain(int32_t param_id, double value_normalized) = 0;
    virtual double plainParamToNormalized(int32_t param_id, double plain_value) = 0;
    virtual TResult setParamNormalized(int32_t param_id, double value) = 0;
    virtual TResult getParamNormalized(int32_t param_id, double& value) = 0;
    virtual TResult setComponentHandler(FUnknown* handler) = 0;
    virtual FUnknown* createView(const char* name) = 0;
};

/// IEditController2 (optional)
class IEditController2 : public FUnknown {
public:
    static const FUID iid;
    virtual TResult setKnobMode(int32_t mode) = 0;
    virtual TResult openHelp(void* only_for_parameter) = 0;
    virtual TResult openAboutBox(void* parent) = 0;
};

} // namespace Vst
} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_IVSTEDITCONTROLLER_H