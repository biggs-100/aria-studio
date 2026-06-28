#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "audio/audio_buffer.h"
#include "audio/buffer_pool.h"
#include "audio/simd_ops.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace aria;

// ─── AudioBuffer tests ─────────────────────────────────────────

TEST_CASE("AudioBuffer basic operations", "[audio][buffer]") {
    constexpr uint32_t kFrames = 64;
    constexpr uint32_t kChannels = 2;

    // Allocate raw storage for testing
    float ch0[kFrames];
    float ch1[kFrames];
    for (uint32_t i = 0; i < kFrames; ++i) {
        ch0[i] = static_cast<float>(i);
        ch1[i] = static_cast<float>(i * 2);
    }

    AudioBuffer buf;
    buf.frames = kFrames;
    buf.channels = kChannels;
    buf.capacity = kFrames;
    buf.data[0] = ch0;
    buf.data[1] = ch1;

    SECTION("channel() returns correct pointers") {
        REQUIRE(buf.channel(0) == ch0);
        REQUIRE(buf.channel(1) == ch1);
        REQUIRE(buf.channel(2) == nullptr);  // out of range
    }

    SECTION("channel() const returns correct pointers") {
        const AudioBuffer& cbuf = buf;
        REQUIRE(cbuf.channel(0) == ch0);
        REQUIRE(cbuf.channel(1) == ch1);
        REQUIRE(cbuf.channel(2) == nullptr);
    }

    SECTION("clear() zeroes all channels") {
        buf.clear();
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(ch0[i] == 0.0f);
            REQUIRE(ch1[i] == 0.0f);
        }
    }

    SECTION("copy_from copies data") {
        AudioBuffer dst;
        float dst0[kFrames] = {};
        float dst1[kFrames] = {};
        dst.frames = kFrames;
        dst.channels = kChannels;
        dst.capacity = kFrames;
        dst.data[0] = dst0;
        dst.data[1] = dst1;

        dst.copy_from(buf);

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(dst0[i] == Catch::Approx(static_cast<float>(i)));
            REQUIRE(dst1[i] == Catch::Approx(static_cast<float>(i * 2)));
        }
    }

    SECTION("add_from accumulates with gain") {
        AudioBuffer dst;
        float dst0[kFrames] = {};
        float dst1[kFrames] = {};
        dst.frames = kFrames;
        dst.channels = kChannels;
        dst.data[0] = dst0;
        dst.data[1] = dst1;

        // Fill dst with some data
        for (uint32_t i = 0; i < kFrames; ++i) {
            dst0[i] = 1.0f;
            dst1[i] = 2.0f;
        }

        dst.add_from(buf, 0.5f);

        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(dst0[i] == Catch::Approx(1.0f + static_cast<float>(i) * 0.5f));
            REQUIRE(dst1[i] == Catch::Approx(2.0f + static_cast<float>(i)));
        }
    }

    SECTION("apply_gain multiplies all samples") {
        buf.apply_gain(0.5f);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(ch0[i] == Catch::Approx(static_cast<float>(i) * 0.5f));
            REQUIRE(ch1[i] == Catch::Approx(static_cast<float>(i)));
        }
    }
}

// ─── MixBuffer tests ───────────────────────────────────────────

TEST_CASE("MixBuffer operations", "[audio][buffer]") {
    constexpr uint32_t kFrames = 32;
    double ch0[kFrames];
    double ch1[kFrames];

    for (uint32_t i = 0; i < kFrames; ++i) {
        ch0[i] = static_cast<double>(i) * 0.5;
        ch1[i] = static_cast<double>(i) * 1.5;
    }

    MixBuffer mb;
    mb.frames = kFrames;
    mb.channels = 2;
    mb.data[0] = ch0;
    mb.data[1] = ch1;

    SECTION("channel access") {
        REQUIRE(mb.channel(0) == ch0);
        REQUIRE(mb.channel(1) == ch1);
        REQUIRE(mb.channel(2) == nullptr);
    }

    SECTION("clear zeroes data") {
        mb.clear();
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(ch0[i] == 0.0);
            REQUIRE(ch1[i] == 0.0);
        }
    }
}

// ─── SIMD ops tests ────────────────────────────────────────────

TEST_CASE("SIMD clear", "[audio][simd]") {
    constexpr uint32_t kFrames = 128;
    float buf1[kFrames];
    float buf2[kFrames];

    // Fill with garbage
    for (auto& v : buf1) v = 42.0f;
    for (auto& v : buf2) v = 42.0f;

    SECTION("scalar clear") {
        simd_clear_scalar(buf1, kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(buf1[i] == 0.0f);
        }
    }

    SECTION("SSE2 clear") {
        simd_clear_sse2(buf2, kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(buf2[i] == 0.0f);
        }
    }

    SECTION("dispatched clear") {
        simd_clear(buf1, kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(buf1[i] == 0.0f);
        }
    }
}

TEST_CASE("SIMD copy", "[audio][simd]") {
    constexpr uint32_t kFrames = 128;
    float src[kFrames];
    float dst[kFrames];

    for (uint32_t i = 0; i < kFrames; ++i) {
        src[i] = static_cast<float>(i) * 0.5f;
    }

    SECTION("scalar copy matches reference") {
        simd_copy_scalar(dst, src, kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(dst[i] == src[i]);
        }
    }

    SECTION("SSE2 copy matches scalar") {
        float scalar_dst[kFrames] = {};
        simd_copy_scalar(scalar_dst, src, kFrames);
        simd_copy_sse2(dst, src, kFrames);
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(dst[i] == scalar_dst[i]);
        }
    }
}

TEST_CASE("SIMD add", "[audio][simd]") {
    constexpr uint32_t kFrames = 130;  // not multiple of 4
    float dst1[kFrames];
    float dst2[kFrames];
    float src[kFrames];

    for (uint32_t i = 0; i < kFrames; ++i) {
        dst1[i] = static_cast<float>(i);
        dst2[i] = static_cast<float>(i);
        src[i] = 1.0f;
    }

    simd_add_scalar(dst1, src, kFrames);
    simd_add_sse2(dst2, src, kFrames);

    SECTION("SSE2 add matches scalar for non-multiple-of-4") {
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(dst2[i] == dst1[i]);
        }
    }
}

TEST_CASE("SIMD multiply", "[audio][simd]") {
    constexpr uint32_t kFrames = 64;
    float buf1[kFrames];
    float buf2[kFrames];

    for (uint32_t i = 0; i < kFrames; ++i) {
        buf1[i] = static_cast<float>(i);
        buf2[i] = static_cast<float>(i);
    }

    simd_mul_scalar(buf1, 0.75f, kFrames);
    simd_mul_sse2(buf2, 0.75f, kFrames);

    SECTION("SSE2 mul matches scalar") {
        for (uint32_t i = 0; i < kFrames; ++i) {
            REQUIRE(buf2[i] == buf1[i]);
        }
    }
}

TEST_CASE("SIMD peak", "[audio][simd]") {
    constexpr uint32_t kFrames = 100;
    float buf[kFrames];

    // Create a signal with known peak
    for (uint32_t i = 0; i < kFrames; ++i) {
        buf[i] = 0.5f * static_cast<float>(i) / kFrames;  // max = 0.5
    }
    buf[50] = 0.95f;  // peak
    buf[51] = 0.85f;

    float sp = simd_peak_scalar(buf, kFrames);
    float sse = simd_peak_sse2(buf, kFrames);

    SECTION("peak value is correct") {
        REQUIRE(sp == Catch::Approx(0.95f));
    }

    SECTION("SSE2 peak matches scalar") {
        REQUIRE(sse == sp);
    }

    SECTION("dispatched peak works") {
        float dp = simd_peak(buf, kFrames);
        REQUIRE(dp == Catch::Approx(0.95f));
    }
}

TEST_CASE("SIMD RMS", "[audio][simd]") {
    constexpr uint32_t kFrames = 80;
    float buf[kFrames];

    // DC signal: all 1.0 → RMS = 1.0
    for (uint32_t i = 0; i < kFrames; ++i) {
        buf[i] = 1.0f;
    }

    double sr = simd_rms_scalar(buf, kFrames);
    double sse = simd_rms_sse2(buf, kFrames);

    SECTION("RMS of DC=1 is 1.0") {
        REQUIRE(sr == Catch::Approx(1.0).margin(1e-10));
    }

    SECTION("SSE2 RMS matches scalar") {
        REQUIRE(sse == Catch::Approx(sr).margin(1e-10));
    }

    // Sine wave
    for (uint32_t i = 0; i < kFrames; ++i) {
        buf[i] = std::sin(2.0f * 3.14159f * 5.0f * static_cast<float>(i) / kFrames);
    }

    double sine_rms_scalar = simd_rms_scalar(buf, kFrames);
    double sine_rms_sse = simd_rms_sse2(buf, kFrames);

    SECTION("SSE2 RMS of sine matches scalar") {
        REQUIRE(sine_rms_sse == Catch::Approx(sine_rms_scalar).margin(1e-10));
    }

    SECTION("dispatched RMS works") {
        double dr = simd_rms(buf, kFrames);
        REQUIRE(dr == Catch::Approx(sine_rms_scalar).margin(1e-10));
    }
}

// ─── LockFreeBufferPool tests ──────────────────────────────────

TEST_CASE("LockFreeBufferPool lifecycle", "[audio][pool]") {
    LockFreeBufferPool pool;

    SECTION("uninitialized pool returns nullptr") {
        REQUIRE_FALSE(pool.initialized());
        REQUIRE(pool.acquire() == nullptr);
    }

    SECTION("init and basic acquire/release") {
        REQUIRE(pool.init(64, 2, 16));
        REQUIRE(pool.initialized());
        REQUIRE(pool.capacity() >= 16);

        AudioBuffer* buf = pool.acquire();
        REQUIRE(buf != nullptr);
        REQUIRE(buf->channels == 2);
        REQUIRE(buf->capacity >= 64);

        pool.release(buf);

        // Can acquire again
        AudioBuffer* buf2 = pool.acquire();
        REQUIRE(buf2 != nullptr);
        pool.release(buf2);
    }

    SECTION("acquire returns all buffers") {
        REQUIRE(pool.init(64, 2, 8));

        std::vector<AudioBuffer*> bufs;
        AudioBuffer* b;
        while ((b = pool.acquire()) != nullptr) {
            bufs.push_back(b);
        }
        // Pool should have exhausted
        REQUIRE(pool.acquire() == nullptr);

        // Release all
        for (auto* buf : bufs) {
            pool.release(buf);
        }

        // Can acquire again
        REQUIRE(pool.acquire() != nullptr);
    }

    SECTION("multiple acquire/release cycles") {
        REQUIRE(pool.init(64, 2, 4));

        for (int cycle = 0; cycle < 10; ++cycle) {
            AudioBuffer* b1 = pool.acquire();
            AudioBuffer* b2 = pool.acquire();
            REQUIRE(b1 != nullptr);
            REQUIRE(b2 != nullptr);
            REQUIRE(b1 != b2);

            pool.release(b1);
            pool.release(b2);
        }
    }

    SECTION("buffer data is usable after acquire") {
        REQUIRE(pool.init(64, 2, 4));

        AudioBuffer* buf = pool.acquire();
        REQUIRE(buf != nullptr);

        buf->frames = 64;
        buf->channels = 2;
        for (uint32_t c = 0; c < 2; ++c) {
            REQUIRE(buf->data[c] != nullptr);
            for (uint32_t i = 0; i < 64; ++i) {
                buf->data[c][i] = static_cast<float>(i);
            }
        }

        pool.release(buf);
    }
}

TEST_CASE("SIMD detection and dispatch table", "[audio][simd]") {
    SimdLevel level = detect_simd_level();
    REQUIRE(level >= SimdLevel::Scalar);

    // On x86-64, SSE2 is guaranteed
#if defined(__x86_64__) || defined(_M_X64)
    REQUIRE(level >= SimdLevel::SSE2);
#endif

    // Dispatch table should be populated
    REQUIRE(g_simd.add != nullptr);
    REQUIRE(g_simd.mul != nullptr);
    REQUIRE(g_simd.copy != nullptr);
    REQUIRE(g_simd.clear != nullptr);
    REQUIRE(g_simd.peak != nullptr);
    REQUIRE(g_simd.rms != nullptr);
}
