#ifndef ARIA_VENDOR_IVSTSTREAM_H
#define ARIA_VENDOR_IVSTSTREAM_H

#if defined(ARIA_FEATURE_VST3)

#include "../base/funknown.h"

namespace Steinberg {

/// IStream (base)
class IStream : public FUnknown {
public:
    static const FUID iid;
    virtual int32_t read(void* buffer, int32_t num_bytes) = 0;
    virtual int32_t write(void* buffer, int32_t num_bytes) = 0;
    virtual int64_t seek(int64_t pos, int32_t mode) = 0;
    virtual int64_t tell() = 0;
};

/// IBStream (VST3 state stream)
class IBStream : public FUnknown {
public:
    static const FUID iid;
    virtual int32_t read(void* buffer, int32_t num_bytes) = 0;
    virtual int32_t write(const void* buffer, int32_t num_bytes) = 0;
    virtual int64_t seek(int64_t pos, int32_t mode) = 0;
    virtual int64_t tell() = 0;
};

} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_IVSTSTREAM_H