#ifndef ARIA_VENDOR_FUNKNOWN_H
#define ARIA_VENDOR_FUNKNOWN_H

#if defined(ARIA_FEATURE_VST3)

#include <cstdint>

namespace Steinberg {

using TBool = int32_t;
using TChar = char16_t;
using int16 = int16_t;
using int32 = int32_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

class FUID {
public:
    FUID() { clear(); }
    explicit FUID(uint64_t d1, uint64_t d2) {
        data_[0] = d1;
        data_[1] = d2;
    }
    bool operator==(const FUID& other) const {
        return data_[0] == other.data_[0] && data_[1] == other.data_[1];
    }
    bool operator!=(const FUID& other) const { return !(*this == other); }
    bool is_valid() const {
        return data_[0] != 0 || data_[1] != 0;
    }
    void clear() {
        data_[0] = 0;
        data_[1] = 0;
    }
private:
    uint64_t data_[2]{};
};

/// Interface ID
using TUID = FUID;

/// Result codes
enum TResult : int32_t {
    kResultOk = 0,
    kResultFalse = -1,
    kResultNoMemory = -2,
    kResultFail = -3,
    kInvalidArgument = -4,
    kInternalError = -5,
    kNotImplemented = -6
};

/// Base interface (COM-like)
class FUnknown {
public:
    virtual ~FUnknown() = default;
    virtual TResult queryInterface(const TUID& _iid, void** obj) = 0;
    virtual uint32_t addRef() = 0;
    virtual uint32_t release() = 0;

    template <typename T>
    T* queryInterface() {
        void* ptr = nullptr;
        if (queryInterface(T::iid, &ptr) == kResultOk && ptr) {
            return static_cast<T*>(ptr);
        }
        return nullptr;
    }
};

/// Smart pointer for FUnknown
template <typename T>
class FPtr {
public:
    FPtr() noexcept : ptr_(nullptr) {}
    explicit FPtr(T* p) noexcept : ptr_(p) {
        if (ptr_) ptr_->addRef();
    }
    FPtr(const FPtr& other) noexcept : ptr_(other.ptr_) {
        if (ptr_) ptr_->addRef();
    }
    FPtr(FPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    ~FPtr() noexcept {
        if (ptr_) ptr_->release();
    }
    FPtr& operator=(const FPtr& other) noexcept {
        if (this != &other) {
            if (ptr_) ptr_->release();
            ptr_ = other.ptr_;
            if (ptr_) ptr_->addRef();
        }
        return *this;
    }
    T* get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    T* ptr_;
};

} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_FUNKNOWN_H