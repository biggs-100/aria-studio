#ifndef ARIA_VENDOR_FMODULE_H
#define ARIA_VENDOR_FMODULE_H

#if defined(ARIA_FEATURE_VST3)

#include "funknown.h"

namespace Steinberg {

/// Module handling for VST3 plugin loading
/// In a real build this comes from the VST3 SDK.
/// Here we provide minimal declarations for compilation.

class Module {
public:
    virtual ~Module() = default;

    /// Platform-specific handle types
    using PlatformPtr = void*;

    /// Module reference
    PlatformPtr ref_ = nullptr;

    /// Factory function type used by VST3 modules
    using GetFactoryProc = IPluginFactory* (*)();
};

} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_FMODULE_H