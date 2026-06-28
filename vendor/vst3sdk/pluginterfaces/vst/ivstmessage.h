#ifndef ARIA_VENDOR_IVSTMESSAGE_H
#define ARIA_VENDOR_IVSTMESSAGE_H

#if defined(ARIA_FEATURE_VST3)

#include "../base/funknown.h"

namespace Steinberg {
namespace Vst {

class IAttributeList : public FUnknown {
public:
    static const FUID iid;

    virtual TResult setInt(const char* id, int64 value) = 0;
    virtual TResult getInt(const char* id, int64& value) = 0;
    virtual TResult setFloat(const char* id, double value) = 0;
    virtual TResult getFloat(const char* id, double& value) = 0;
    virtual TResult setString(const char* id, const TChar* string) = 0;
    virtual TResult getString(const char* id, TChar* string, uint32_t size) = 0;
    virtual TResult setBinary(const char* id, const void* data, uint32_t size) = 0;
    virtual TResult getBinary(const char* id, void* data, uint32_t& size) = 0;
};

class IMessage : public FUnknown {
public:
    static const FUID iid;

    virtual const char* getMessageID() = 0;
    virtual void setMessageID(const char* id) = 0;
    virtual IAttributeList* getAttributes() = 0;
};

class IConnectionPoint : public FUnknown {
public:
    static const FUID iid;

    virtual TResult connect(IConnectionPoint* other) = 0;
    virtual TResult disconnect(IConnectionPoint* other) = 0;
    virtual TResult notify(IMessage* message) = 0;
};

} // namespace Vst
} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_IVSTMESSAGE_H