#ifndef ARIA_SIMD_OPS_H
#define ARIA_SIMD_OPS_H

#include <cstdint>

namespace aria {

/// Runtime-detected SIMD capability level.
enum class SimdLevel {
    Scalar,   ///< No SIMD — portable fallback
    SSE2,     ///< x86 SSE2 (baseline for x86-64)
    SSE4_1,   ///< x86 SSE4.1
    AVX,      ///< x86 AVX (128/256-bit)
    AVX2,     ///< x86 AVX2 + FMA
    AVX512,   ///< x86 AVX-512
    NEON,     ///< ARM NEON (32-bit)
    NEON64    ///< ARM AArch64 NEON
};

/// Detect the highest available SIMD level on the current CPU.
SimdLevel detect_simd_level();

/// @name Vector buffer operations
/// Each function is available in two forms:
///   - `simd_*(...)` — uses the highest detected SIMD level
///   - `simd_*_scalar(...)` — always uses portable scalar code
///   - `simd_*_sse2(...)` — always uses SSE2 (x86 only)
///
/// All operations process the first @p frames samples.
/// Pointers MUST be non-null and 16-byte aligned for best SIMD performance
/// (unaligned loads/stores are handled gracefully).
///@{

/// dst[i] += src[i]  for i in [0, frames)
void simd_add(float* dst, const float* src, uint32_t frames);
void simd_add_scalar(float* dst, const float* src, uint32_t frames);
void simd_add_sse2(float* dst, const float* src, uint32_t frames);

/// dst[i] *= gain  for i in [0, frames)
void simd_mul(float* dst, float gain, uint32_t frames);
void simd_mul_scalar(float* dst, float gain, uint32_t frames);
void simd_mul_sse2(float* dst, float gain, uint32_t frames);

/// dst[i] = src[i]  for i in [0, frames)
void simd_copy(float* dst, const float* src, uint32_t frames);
void simd_copy_scalar(float* dst, const float* src, uint32_t frames);
void simd_copy_sse2(float* dst, const float* src, uint32_t frames);

/// dst[i] = 0.0f  for i in [0, frames)
void simd_clear(float* dst, uint32_t frames);
void simd_clear_scalar(float* dst, uint32_t frames);
void simd_clear_sse2(float* dst, uint32_t frames);

/// @return max(|buf[i]|) for i in [0, frames)
float simd_peak(const float* buf, uint32_t frames);
float simd_peak_scalar(const float* buf, uint32_t frames);
float simd_peak_sse2(const float* buf, uint32_t frames);

/// @return sqrt(mean(buf[i]^2)) for i in [0, frames)
double simd_rms(const float* buf, uint32_t frames);
double simd_rms_scalar(const float* buf, uint32_t frames);
double simd_rms_sse2(const float* buf, uint32_t frames);

/// Multi-channel versions — apply operation to all channels.
void simd_clear_multi(float** dst, uint32_t channels, uint32_t frames);
void simd_add_multi(float** dst, float** src, uint32_t channels, uint32_t frames);

///@}

/// Function pointer table set by detect_simd_level().
/// Initialized to scalar fallbacks; call detect_simd_level() to upgrade.
struct SimdDispatchTable {
    void (*add)(float* dst, const float* src, uint32_t frames) = simd_add_scalar;
    void (*mul)(float* dst, float gain, uint32_t frames) = simd_mul_scalar;
    void (*copy)(float* dst, const float* src, uint32_t frames) = simd_copy_scalar;
    void (*clear)(float* dst, uint32_t frames) = simd_clear_scalar;
    float (*peak)(const float* buf, uint32_t frames) = simd_peak_scalar;
    double (*rms)(const float* buf, uint32_t frames) = simd_rms_scalar;
};

/// Global dispatch table populated by detect_simd_level().
extern SimdDispatchTable g_simd;

} // namespace aria

#endif // ARIA_SIMD_OPS_H
