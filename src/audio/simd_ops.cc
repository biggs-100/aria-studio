#include "simd_ops.h"

// Platform-specific SIMD intrinsics
#if defined(_MSC_VER)
#  include <intrin.h>
#else
#  include <xmmintrin.h>   // SSE
#  include <emmintrin.h>   // SSE2
#  include <pmmintrin.h>   // SSE3
#  include <smmintrin.h>   // SSE4.1
#endif

#include <cmath>
#include <cstring>
#include <algorithm>

namespace aria {

// ─── Dispatch table ────────────────────────────────────────────
SimdDispatchTable g_simd;

// ─── CPU feature detection ─────────────────────────────────────

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#  include <intrin.h>
static inline void cpuid(int info[4], int function_id) {
    __cpuidex(info, function_id, 0);
}

static uint64_t xgetbv(uint32_t xcr) {
    return _xgetbv(xcr);
}
#elif defined(__x86_64__) || defined(__i386__)
static inline void cpuid(int info[4], int function_id) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
        : "a"(function_id), "c"(0)
    );
}

static uint64_t xgetbv(uint32_t xcr) {
    uint32_t eax, edx;
    __asm__ __volatile__(
        "xgetbv"
        : "=a"(eax), "=d"(edx)
        : "c"(xcr)
    );
    return (static_cast<uint64_t>(edx) << 32) | eax;
}
#else
// ARM or other — no x86 CPUID
static inline void cpuid(int[4], int) {}
static uint64_t xgetbv(uint32_t) { return 0; }
#endif

SimdLevel detect_simd_level() {
#if defined(__ARM_NEON) || defined(__aarch64__)
#  if defined(__aarch64__)
    g_simd.add   = simd_add_sse2;    // reuse SSE2 path on NEON via portable fallback
    g_simd.mul   = simd_mul_sse2;
    g_simd.copy  = simd_copy_sse2;
    g_simd.clear = simd_clear_sse2;
    g_simd.peak  = simd_peak_sse2;
    g_simd.rms   = simd_rms_sse2;
    return SimdLevel::NEON64;
#  else
    // 32-bit ARM NEON fallback to scalar
    return SimdLevel::NEON;
#  endif
#endif

    // x86 CPUID detection
    int info[4] = {};

    // Check max function
    cpuid(info, 0);
    int max_id = info[0];
    if (max_id < 1) return SimdLevel::Scalar;

    cpuid(info, 1);
    bool sse2   = (info[3] & (1 << 26)) != 0;
    bool sse41  = (info[2] & (1 << 19)) != 0;
    bool avx    = (info[2] & (1 << 28)) != 0;

    bool osxsave = (info[2] & (1 << 27)) != 0;

    bool avx2   = false;
    bool avx512 = false;

    if (osxsave && (xgetbv(0) & 6) == 6) {
        // XMM and YMM state enabled by OS
        if (max_id >= 7) {
            cpuid(info, 7);
            avx2   = (info[1] & (1 << 5)) != 0;

            // AVX-512 foundation + mask + zmm state check
            bool avx512f    = (info[1] & (1 << 16)) != 0;
            bool avx512cd   = (info[1] & (1 << 28)) != 0;
            bool avx512bw   = (info[1] & (1 << 30)) != 0;
            bool avx512dq   = (info[1] & (1 << 17)) != 0;

            // Check if upper 256-bit state is enabled (ZMM)
            if ((xgetbv(0) & 0xE0) == 0xE0) {
                avx512 = avx512f && avx512cd && avx512bw && avx512dq;
            }
        }
    }

    // Configure dispatch table — use best available
    if (avx512) {
        g_simd.add   = simd_add_sse2;  // placeholder — AVX512 impl TBD
        g_simd.mul   = simd_mul_sse2;
        g_simd.copy  = simd_copy_sse2;
        g_simd.clear = simd_clear_sse2;
        g_simd.peak  = simd_peak_sse2;
        g_simd.rms   = simd_rms_sse2;
        return SimdLevel::AVX512;
    }

    if (avx2) {
        g_simd.add   = simd_add_sse2;
        g_simd.mul   = simd_mul_sse2;
        g_simd.copy  = simd_copy_sse2;
        g_simd.clear = simd_clear_sse2;
        g_simd.peak  = simd_peak_sse2;
        g_simd.rms   = simd_rms_sse2;
        return SimdLevel::AVX2;
    }

    if (avx) {
        g_simd.add   = simd_add_sse2;
        g_simd.mul   = simd_mul_sse2;
        g_simd.copy  = simd_copy_sse2;
        g_simd.clear = simd_clear_sse2;
        g_simd.peak  = simd_peak_sse2;
        g_simd.rms   = simd_rms_sse2;
        return SimdLevel::AVX;
    }

    if (sse41) {
        g_simd.add   = simd_add_sse2;
        g_simd.mul   = simd_mul_sse2;
        g_simd.copy  = simd_copy_sse2;
        g_simd.clear = simd_clear_sse2;
        g_simd.peak  = simd_peak_sse2;
        g_simd.rms   = simd_rms_sse2;
        return SimdLevel::SSE4_1;
    }

    if (sse2) {
        g_simd.add   = simd_add_sse2;
        g_simd.mul   = simd_mul_sse2;
        g_simd.copy  = simd_copy_sse2;
        g_simd.clear = simd_clear_sse2;
        g_simd.peak  = simd_peak_sse2;
        g_simd.rms   = simd_rms_sse2;
        return SimdLevel::SSE2;
    }

    return SimdLevel::Scalar;
}

// ─── Scalar implementations ────────────────────────────────────

void simd_add_scalar(float* dst, const float* src, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        dst[i] += src[i];
    }
}

void simd_mul_scalar(float* dst, float gain, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        dst[i] *= gain;
    }
}

void simd_copy_scalar(float* dst, const float* src, uint32_t frames) {
    std::memcpy(dst, src, frames * sizeof(float));
}

void simd_clear_scalar(float* dst, uint32_t frames) {
    std::memset(dst, 0, frames * sizeof(float));
}

float simd_peak_scalar(const float* buf, uint32_t frames) {
    float peak = 0.0f;
    for (uint32_t i = 0; i < frames; ++i) {
        float v = std::abs(buf[i]);
        if (v > peak) peak = v;
    }
    return peak;
}

double simd_rms_scalar(const float* buf, uint32_t frames) {
    double sum = 0.0;
    for (uint32_t i = 0; i < frames; ++i) {
        sum += static_cast<double>(buf[i]) * static_cast<double>(buf[i]);
    }
    return std::sqrt(sum / static_cast<double>(frames));
}

// ─── SSE2 implementations ──────────────────────────────────────

void simd_add_sse2(float* dst, const float* src, uint32_t frames) {
    uint32_t i = 0;
    // Process 4 floats at a time
    for (; i + 4 <= frames; i += 4) {
        __m128 a = _mm_loadu_ps(&dst[i]);
        __m128 b = _mm_loadu_ps(&src[i]);
        __m128 r = _mm_add_ps(a, b);
        _mm_storeu_ps(&dst[i], r);
    }
    // Remainder
    for (; i < frames; ++i) {
        dst[i] += src[i];
    }
}

void simd_mul_sse2(float* dst, float gain, uint32_t frames) {
    __m128 g = _mm_set1_ps(gain);
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        __m128 a = _mm_loadu_ps(&dst[i]);
        __m128 r = _mm_mul_ps(a, g);
        _mm_storeu_ps(&dst[i], r);
    }
    for (; i < frames; ++i) {
        dst[i] *= gain;
    }
}

void simd_copy_sse2(float* dst, const float* src, uint32_t frames) {
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        __m128 s = _mm_loadu_ps(&src[i]);
        _mm_storeu_ps(&dst[i], s);
    }
    for (; i < frames; ++i) {
        dst[i] = src[i];
    }
}

void simd_clear_sse2(float* dst, uint32_t frames) {
    __m128 zero = _mm_setzero_ps();
    uint32_t i = 0;
    for (; i + 4 <= frames; i += 4) {
        _mm_storeu_ps(&dst[i], zero);
    }
    for (; i < frames; ++i) {
        dst[i] = 0.0f;
    }
}

float simd_peak_sse2(const float* buf, uint32_t frames) {
    __m128 peak = _mm_setzero_ps();
    uint32_t i = 0;

    // XOR mask to flip sign bit (absolute value via bitwise trick)
    const __m128 abs_mask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));

    for (; i + 4 <= frames; i += 4) {
        __m128 v = _mm_loadu_ps(&buf[i]);
        v = _mm_and_ps(v, abs_mask);          // |v|
        peak = _mm_max_ps(peak, v);            // max across lanes
    }

    // Horizontal max across the 4 SSE lanes
    float result[4];
    _mm_storeu_ps(result, peak);
    float p = result[0];
    for (int k = 1; k < 4; ++k) {
        if (result[k] > p) p = result[k];
    }

    // Remainder
    for (; i < frames; ++i) {
        float v = std::abs(buf[i]);
        if (v > p) p = v;
    }
    return p;
}

double simd_rms_sse2(const float* buf, uint32_t frames) {
    __m128 sum = _mm_setzero_ps();
    uint32_t i = 0;

    for (; i + 4 <= frames; i += 4) {
        __m128 v = _mm_loadu_ps(&buf[i]);
        __m128 sq = _mm_mul_ps(v, v);
        sum = _mm_add_ps(sum, sq);
    }

    // Horizontal sum
    float tmp[4];
    _mm_storeu_ps(tmp, sum);
    double total = static_cast<double>(tmp[0]) + static_cast<double>(tmp[1])
                 + static_cast<double>(tmp[2]) + static_cast<double>(tmp[3]);

    // Remainder
    for (; i < frames; ++i) {
        total += static_cast<double>(buf[i]) * static_cast<double>(buf[i]);
    }

    return std::sqrt(total / static_cast<double>(frames));
}

// ─── Dispatch wrappers ─────────────────────────────────────────

void simd_add(float* dst, const float* src, uint32_t frames) {
    g_simd.add(dst, src, frames);
}

void simd_mul(float* dst, float gain, uint32_t frames) {
    g_simd.mul(dst, gain, frames);
}

void simd_copy(float* dst, const float* src, uint32_t frames) {
    g_simd.copy(dst, src, frames);
}

void simd_clear(float* dst, uint32_t frames) {
    g_simd.clear(dst, frames);
}

float simd_peak(const float* buf, uint32_t frames) {
    return g_simd.peak(buf, frames);
}

double simd_rms(const float* buf, uint32_t frames) {
    return g_simd.rms(buf, frames);
}

// ─── Multi-channel helpers ─────────────────────────────────────

void simd_clear_multi(float** dst, uint32_t channels, uint32_t frames) {
    for (uint32_t c = 0; c < channels; ++c) {
        if (dst[c]) simd_clear(dst[c], frames);
    }
}

void simd_add_multi(float** dst, float** src, uint32_t channels, uint32_t frames) {
    for (uint32_t c = 0; c < channels; ++c) {
        if (dst[c] && src[c]) simd_add(dst[c], src[c], frames);
    }
}

} // namespace aria
