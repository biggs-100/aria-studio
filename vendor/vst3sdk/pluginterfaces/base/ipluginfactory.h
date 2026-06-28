#ifndef ARIA_VENDOR_IPLUGINFACTORY_H
#define ARIA_VENDOR_IPLUGINFACTORY_H

#if defined(ARIA_FEATURE_VST3)

#include "funknown.h"

namespace Steinberg {

class PClassInfo {
public:
    PClassInfo() { clear(); }

    void clear() {
        category_[0] = 0;
        name_[0] = 0;
        vendor_[0] = 0;
        version_[0] = 0;
        sdk_version_[0] = 0;
        subcategories_[0] = 0;
    }

    TUID cid;
    TChar category_[32];
    TChar name_[64];
    TChar vendor_[64];
    TChar version_[64];
    TChar sdk_version_[64];
    TChar subcategories_[128];
};

class PClassInfoW {
public:
    PClassInfoW() { clear(); }

    void clear() {
        category_[0] = 0;
        name_[0] = 0;
        vendor_[0] = 0;
        version_[0] = 0;
        sdk_version_[0] = 0;
        subcategories_[0] = 0;
    }

    TUID cid;
    TChar category_[32];
    TChar name_[64];
    TChar vendor_[64];
    TChar version_[64];
    TChar sdk_version_[64];
    TChar subcategories_[128];
    uint32_t class_flags_ = 0;
};

/// IPluginFactory interface
class IPluginFactory : public FUnknown {
public:
    static const FUID iid;

    virtual int32_t countClasses() = 0;
    virtual TResult getClassInfo(int32_t index, PClassInfo* info) = 0;
    virtual FUnknown* createInstance(const TUID& cid, const TUID& _iid) = 0;
};

/// IPluginFactory2 (optional)
class IPluginFactory2 : public IPluginFactory {
public:
    static const FUID iid;
    virtual TResult getClassInfo2(int32_t index, PClassInfoW* info) = 0;
};

/// IPluginFactory3 (optional)
class IPluginFactory3 : public IPluginFactory2 {
public:
    static const FUID iid;
    virtual TResult setHostContext(FUnknown* context) = 0;
};

} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_IPLUGINFACTORY_H