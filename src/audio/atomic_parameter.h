#ifndef ARIA_ATOMIC_PARAMETER_H
#define ARIA_ATOMIC_PARAMETER_H

#include <atomic>
#include <cstdint>

namespace aria {

/// Lock-free parameter storage for audio-thread-safe parameter updates.
///
/// Uses double-buffering via std::atomic<double> for lock-free reads and
/// writes. The audio thread reads the current value with relaxed ordering
/// (monotonic within the callback). The control thread writes with release
/// ordering.
///
/// For parameters that need coordinated multi-value updates (e.g., gain and
/// ramp time), use multiple AtomicParameter instances or a custom atomic
/// struct with a generation counter.
class AtomicParameter {
public:
    AtomicParameter() = default;
    explicit AtomicParameter(double initial) : value_(initial) {}

    /// Store a new parameter value (control thread).
    void store(double v, std::memory_order order = std::memory_order_release) {
        value_.store(v, order);
    }

    /// Read current parameter value (audio thread).
    double load(std::memory_order order = std::memory_order_relaxed) const {
        return value_.load(order);
    }

    /// Compare-and-swap (control thread).
    bool compare_exchange(double expected, double desired) {
        return value_.compare_exchange_weak(expected, desired,
                                            std::memory_order_release,
                                            std::memory_order_relaxed);
    }

    /// Implicit conversion for audio-thread reads.
    operator double() const { return load(); }

    /// Assignment from control thread.
    AtomicParameter& operator=(double v) {
        store(v);
        return *this;
    }

private:
    std::atomic<double> value_{0.0};
};

} // namespace aria

#endif // ARIA_ATOMIC_PARAMETER_H
