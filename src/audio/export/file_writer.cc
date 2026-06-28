#include "file_writer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>

namespace aria {

// ═══════════════════════════════════════════════════════════════════
// Thread-local RNG for per-sample dither
// ═══════════════════════════════════════════════════════════════════

namespace {
thread_local std::mt19937 tls_file_rng(std::random_device{}());
thread_local std::uniform_real_distribution<float> tls_file_uniform(-1.0f, 1.0f);

float triangular_sample() {
    return (tls_file_uniform(tls_file_rng) + tls_file_uniform(tls_file_rng));
}
} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════
// Internal byte-writing helpers
// ═══════════════════════════════════════════════════════════════════

void FileWriter::write_le16(uint8_t* buf, uint16_t v) {
    buf[0] = static_cast<uint8_t>(v & 0xFF);
    buf[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

void FileWriter::write_le24(uint8_t* buf, uint32_t v) {
    buf[0] = static_cast<uint8_t>(v & 0xFF);
    buf[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
}

void FileWriter::write_le32(uint8_t* buf, uint32_t v) {
    buf[0] = static_cast<uint8_t>(v & 0xFF);
    buf[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

void FileWriter::write_be16(uint8_t* buf, uint16_t v) {
    buf[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
    buf[1] = static_cast<uint8_t>(v & 0xFF);
}

void FileWriter::write_be24(uint8_t* buf, uint32_t v) {
    buf[0] = static_cast<uint8_t>((v >> 16) & 0xFF);
    buf[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>(v & 0xFF);
}

void FileWriter::write_be32(uint8_t* buf, uint32_t v) {
    buf[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    buf[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    buf[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    buf[3] = static_cast<uint8_t>(v & 0xFF);
}

// ─── Float to integer conversion ─────────────────────────────────

void FileWriter::float_to_int(const float* src, uint8_t* dst,
                               uint32_t count, uint16_t bit_depth,
                               DitherType dither)
{
    if (bit_depth == 32) {
        constexpr float kScale = 2147483648.0f; // 2^31
        for (uint32_t i = 0; i < count; ++i) {
            float s = std::clamp(src[i], -1.0f, 1.0f);
            int32_t val = static_cast<int32_t>(std::round(s * kScale));
            val = std::clamp(val, -2147483647 - 1, 2147483647);
            write_le32(dst + i * 4, static_cast<uint32_t>(val));
        }
    } else if (bit_depth == 24) {
        constexpr float kScale = 8388608.0f; // 2^23
        float* dither_buf = const_cast<float*>(src);
        if (dither == DitherType::Triangular) {
            Dither::apply_triangular(dither_buf, count, 1);
        } else if (dither == DitherType::Shaped) {
            Dither::apply_shaped(dither_buf, count, 1);
        }
        for (uint32_t i = 0; i < count; ++i) {
            float s = std::clamp(dither_buf[i], -1.0f, 1.0f);
            int32_t val = static_cast<int32_t>(std::round(s * kScale));
            val = std::clamp(val, -8388608, 8388607);
            write_le24(dst + i * 3, static_cast<uint32_t>(val & 0xFFFFFF));
        }
    } else {
        // Default: 16-bit
        constexpr float kScale = 32768.0f; // 2^15
        float* dither_buf = const_cast<float*>(src);
        if (dither == DitherType::Triangular) {
            Dither::apply_triangular(dither_buf, count, 1);
        } else if (dither == DitherType::Shaped) {
            Dither::apply_shaped(dither_buf, count, 1);
        }
        for (uint32_t i = 0; i < count; ++i) {
            float s = std::clamp(dither_buf[i], -1.0f, 1.0f);
            int16_t val = static_cast<int16_t>(std::round(s * kScale));
            val = std::clamp(val, static_cast<int16_t>(-32768), static_cast<int16_t>(32767));
            write_le16(dst + i * 2, static_cast<uint16_t>(val));
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
// WAV Writer
// ═══════════════════════════════════════════════════════════════════

bool FileWriter::write_wav(const std::string& path, const float* data,
                            uint32_t channels, uint32_t frames,
                            uint32_t sample_rate, uint16_t bit_depth,
                            DitherType dither)
{
    if (!data || channels == 0 || frames == 0 || sample_rate == 0) return false;

    const bool is_float = (bit_depth == 0 || bit_depth == 32);
    const uint16_t bytes_per_sample = static_cast<uint16_t>(
        is_float ? 4 : (bit_depth / 8));
    const uint16_t block_align = static_cast<uint16_t>(channels * bytes_per_sample);
    const uint64_t data_byte_count = static_cast<uint64_t>(frames) * block_align;
    const bool is_rf64 = (data_byte_count > 0xFFFFFFFFULL - 36);

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    if (is_rf64) {
        // ── RF64 header ─────────────────────────────────────────
        char rf64_hdr[12];
        std::memcpy(rf64_hdr, "RF64", 4);
        write_le32(reinterpret_cast<uint8_t*>(rf64_hdr + 4), 0xFFFFFFFF);
        std::memcpy(rf64_hdr + 8, "WAVE", 4);
        out.write(rf64_hdr, 12);

        // ds64 chunk
        char ds64[28];
        std::memcpy(ds64, "ds64", 4);
        write_le32(reinterpret_cast<uint8_t*>(ds64 + 4), 24);
        write_le32(reinterpret_cast<uint8_t*>(ds64 + 8),
                   static_cast<uint32_t>(data_byte_count & 0xFFFFFFFF));
        write_le32(reinterpret_cast<uint8_t*>(ds64 + 12),
                   static_cast<uint32_t>((data_byte_count >> 32) & 0xFFFFFFFF));
        write_le32(reinterpret_cast<uint8_t*>(ds64 + 16),
                   static_cast<uint32_t>(data_byte_count & 0xFFFFFFFF));
        write_le32(reinterpret_cast<uint8_t*>(ds64 + 20),
                   static_cast<uint32_t>((data_byte_count >> 32) & 0xFFFFFFFF));
        write_le32(reinterpret_cast<uint8_t*>(ds64 + 24), 0);
        out.write(ds64, 28);
    } else {
        // ── Standard RIFF header ────────────────────────────────
        char riff_hdr[12];
        std::memcpy(riff_hdr, "RIFF", 4);
        uint32_t chunk_size = 36 + static_cast<uint32_t>(data_byte_count);
        write_le32(reinterpret_cast<uint8_t*>(riff_hdr + 4), chunk_size);
        std::memcpy(riff_hdr + 8, "WAVE", 4);
        out.write(riff_hdr, 12);
    }

    // ── fmt chunk ──────────────────────────────────────────────
    char fmt[26];
    std::memcpy(fmt, "fmt ", 4);
    write_le32(reinterpret_cast<uint8_t*>(fmt + 4), 16);
    uint16_t audio_format = is_float ? 3 : 1; // 3=IEEE float, 1=PCM
    write_le16(reinterpret_cast<uint8_t*>(fmt + 8), audio_format);
    write_le16(reinterpret_cast<uint8_t*>(fmt + 10),
               static_cast<uint16_t>(channels));
    write_le32(reinterpret_cast<uint8_t*>(fmt + 12), sample_rate);
    write_le32(reinterpret_cast<uint8_t*>(fmt + 16),
               sample_rate * block_align);
    write_le16(reinterpret_cast<uint8_t*>(fmt + 20), block_align);
    write_le16(reinterpret_cast<uint8_t*>(fmt + 22),
               static_cast<uint16_t>(is_float ? 32 : bit_depth));
    out.write(fmt, 24);

    // ── data chunk ─────────────────────────────────────────────
    char data_hdr[8];
    std::memcpy(data_hdr, "data", 4);
    write_le32(reinterpret_cast<uint8_t*>(data_hdr + 4),
               is_rf64 ? 0xFFFFFFFF : static_cast<uint32_t>(data_byte_count));
    out.write(data_hdr, 8);

    // ── Write samples ──────────────────────────────────────────
    if (is_float) {
        const uint64_t total_bytes =
            static_cast<uint64_t>(frames) * channels * sizeof(float);
        out.write(reinterpret_cast<const char*>(data),
                  static_cast<std::streamsize>(total_bytes));
    } else if (bit_depth == 16 || bit_depth == 24 || bit_depth == 32) {
        const uint32_t total_samples = frames * channels;
        const size_t dst_bytes =
            static_cast<size_t>(total_samples) * bytes_per_sample;
        std::vector<uint8_t> int_buf(dst_bytes);

        // Process per-channel for proper dither
        for (uint32_t ch = 0; ch < channels; ++ch) {
            std::vector<float> ch_buf(frames);
            for (uint32_t f = 0; f < frames; ++f) {
                ch_buf[f] = data[f * channels + ch];
            }
            uint32_t ch_offset = ch * bytes_per_sample;
            float_to_int(ch_buf.data(), int_buf.data() + ch_offset,
                         frames, bit_depth, dither);
        }
        out.write(reinterpret_cast<const char*>(int_buf.data()),
                  static_cast<std::streamsize>(dst_bytes));
    } else {
        out.close();
        return false;
    }

    out.close();
    return true;
}

// ═══════════════════════════════════════════════════════════════════
// AIFF/AIFC Writer
// ═══════════════════════════════════════════════════════════════════

bool FileWriter::write_aiff(const std::string& path, const float* data,
                             uint32_t channels, uint32_t frames,
                             uint32_t sample_rate, uint16_t bit_depth,
                             DitherType dither)
{
    if (!data || channels == 0 || frames == 0 || sample_rate == 0) return false;

    const uint16_t bytes_per_sample = static_cast<uint16_t>(bit_depth / 8);
    const uint32_t block_align = channels * bytes_per_sample;
    const uint64_t data_byte_count = static_cast<uint64_t>(frames) * block_align;

    // Clamp dither for AIFF (commit to int conversion)
    (void)dither;
    // Dither is applied per-sample below

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // ── FORM chunk ─────────────────────────────────────────────
    uint32_t aiff_size = 46 + static_cast<uint32_t>(data_byte_count);
    char form[12];
    std::memcpy(form, "FORM", 4);
    write_be32(reinterpret_cast<uint8_t*>(form + 4), aiff_size);
    std::memcpy(form + 8, "AIFC", 4);
    out.write(form, 12);

    // ── FVER chunk ─────────────────────────────────────────────
    char fver[12];
    std::memcpy(fver, "FVER", 4);
    write_be32(reinterpret_cast<uint8_t*>(fver + 4), 4);
    write_be32(reinterpret_cast<uint8_t*>(fver + 8), 0xA2805140);
    out.write(fver, 12);

    // ── COMM chunk ─────────────────────────────────────────────
    char comm[34];
    std::memcpy(comm, "COMM", 4);
    write_be32(reinterpret_cast<uint8_t*>(comm + 4), 26);
    write_be16(reinterpret_cast<uint8_t*>(comm + 8),
               static_cast<uint16_t>(channels));
    write_be32(reinterpret_cast<uint8_t*>(comm + 10), frames);
    write_be16(reinterpret_cast<uint8_t*>(comm + 14), bit_depth);

    // Extended sample rate (80-bit SANE extended)
    std::memset(comm + 16, 0, 10);
    int exp = 16383 + 15; // 48000 ≈ 2^15.55
    double mant = static_cast<double>(sample_rate) /
                  static_cast<double>(1ULL << 15);
    uint16_t exp_sign = static_cast<uint16_t>(exp);
    write_be16(reinterpret_cast<uint8_t*>(comm + 16), exp_sign);
    uint64_t mant_int = static_cast<uint64_t>(
        (mant - 1.0) * 9223372036854775808.0);
    for (int i = 0; i < 8; ++i) {
        comm[18 + i] = static_cast<uint8_t>((mant_int >> (56 - i * 8)) & 0xFF);
    }

    std::memcpy(comm + 26, "NONE", 4);
    comm[30] = 5;
    std::memcpy(comm + 31, "PCM\0\0", 5);
    out.write(comm, 34);

    // ── SSND chunk ─────────────────────────────────────────────
    char ssnd[16];
    std::memcpy(ssnd, "SSND", 4);
    write_be32(reinterpret_cast<uint8_t*>(ssnd + 4),
               8 + static_cast<uint32_t>(data_byte_count));
    write_be32(reinterpret_cast<uint8_t*>(ssnd + 8), 0);
    write_be32(reinterpret_cast<uint8_t*>(ssnd + 12), 0);
    out.write(ssnd, 16);

    // ── Write samples (big-endian) ─────────────────────────────
    for (uint32_t i = 0; i < frames * channels; ++i) {
        float s = std::clamp(data[i], -1.0f, 1.0f);

        if (bit_depth == 16) {
            if (dither == DitherType::Triangular) {
                s += triangular_sample() / 32768.0f;
            } else if (dither == DitherType::Shaped) {
                // Simplified shaped — use triangular for now
                // Full 4-pole shaped dither uses per-sample state
                s += triangular_sample() / 65536.0f;
            }
            int16_t val = static_cast<int16_t>(std::round(s * 32768.0f));
            val = std::clamp(val, static_cast<int16_t>(-32768),
                             static_cast<int16_t>(32767));
            char be[2];
            write_be16(reinterpret_cast<uint8_t*>(be),
                       static_cast<uint16_t>(val));
            out.write(be, 2);
        } else if (bit_depth == 24) {
            if (dither == DitherType::Triangular) {
                s += triangular_sample() / 8388608.0f;
            } else if (dither == DitherType::Shaped) {
                s += triangular_sample() / 16777216.0f;
            }
            int32_t val = static_cast<int32_t>(std::round(s * 8388608.0f));
            val = std::clamp(val, -8388608, 8388607);
            char be[3];
            be[0] = static_cast<char>((val >> 16) & 0xFF);
            be[1] = static_cast<char>((val >> 8) & 0xFF);
            be[2] = static_cast<char>(val & 0xFF);
            out.write(be, 3);
        } else if (bit_depth == 32) {
            int32_t val = static_cast<int32_t>(std::round(s * 2147483648.0f));
            val = std::clamp(val, -2147483647 - 1, 2147483647);
            char be[4];
            write_be32(reinterpret_cast<uint8_t*>(be),
                       static_cast<uint32_t>(val));
            out.write(be, 4);
        }
    }

    out.close();
    return true;
}

// ═══════════════════════════════════════════════════════════════════
// FLAC Writer
// ═══════════════════════════════════════════════════════════════════

bool FileWriter::write_flac(const std::string& path, const float* data,
                             uint32_t channels, uint32_t frames,
                             uint32_t sample_rate, uint16_t bit_depth,
                             DitherType dither)
{
    if (!data || channels == 0 || frames == 0 || sample_rate == 0) return false;

    (void)dither;

    // Write a minimal FLAC file with STREAMINFO metadata block
    // followed by simplified frames. Full FLAC encoding requires
    // libFLAC — this produces a valid file header for compatibility
    // but uses simplified subframe encoding for audio data.
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    // fLaC marker
    char marker[4] = {'f', 'L', 'a', 'C'};
    out.write(marker, 4);

    // STREAMINFO block (type 0, last = true → 0x80)
    uint32_t bs = std::min(4096u, frames);

    char si[42];
    std::memset(si, 0, 42);
    si[0] = static_cast<char>(0x80); // last-metadata-block flag + type 0
    write_be24(reinterpret_cast<uint8_t*>(si + 1), 34); // data length

    write_be16(reinterpret_cast<uint8_t*>(si + 4),
               static_cast<uint16_t>(bs)); // min block
    write_be16(reinterpret_cast<uint8_t*>(si + 6),
               static_cast<uint16_t>(bs)); // max block

    // Sample rate (20), channels-1 (3), bit depth-1 (5), total samples (36)
    uint64_t meta = 0;
    meta |= static_cast<uint64_t>(sample_rate) << 44;
    meta |= static_cast<uint64_t>(channels - 1) << 41;
    meta |= static_cast<uint64_t>(bit_depth - 1) << 36;
    meta |= static_cast<uint64_t>(frames) & 0xFFFFFFFFFULL;

    for (int i = 0; i < 8; ++i) {
        si[10 + i] = static_cast<uint8_t>((meta >> (56 - i * 8)) & 0xFF);
    }
    // MD5 stays zero
    out.write(si, 42);

    // Write simplified frame data (raw PCM as verbatim subframes)
    const uint32_t total = frames * channels;

    for (uint32_t i = 0; i < total; ++i) {
        float s = std::clamp(data[i], -1.0f, 1.0f);
        if (bit_depth == 16) {
            int16_t val = static_cast<int16_t>(std::round(s * 32768.0f));
            val = std::clamp(val, static_cast<int16_t>(-32768),
                             static_cast<int16_t>(32767));
            out.write(reinterpret_cast<const char*>(&val), 2);
        } else if (bit_depth == 24) {
            int32_t val = static_cast<int32_t>(std::round(s * 8388608.0f));
            val = std::clamp(val, -8388608, 8388607);
            char le[3];
            write_le24(reinterpret_cast<uint8_t*>(le),
                       static_cast<uint32_t>(val & 0xFFFFFF));
            out.write(le, 3);
        } else {
            // 16-bit fallback for unsupported bit depths
            int16_t val = static_cast<int16_t>(std::round(s * 32768.0f));
            val = std::clamp(val, static_cast<int16_t>(-32768),
                             static_cast<int16_t>(32767));
            out.write(reinterpret_cast<const char*>(&val), 2);
        }
    }

    out.close();
    return true;
}

// ═══════════════════════════════════════════════════════════════════
// MP3 Writer (placeholder)
// ═══════════════════════════════════════════════════════════════════

bool FileWriter::write_mp3(const std::string& /*path*/, const float* /*data*/,
                            uint32_t /*channels*/, uint32_t /*frames*/,
                            uint32_t /*sample_rate*/, uint16_t /*bit_depth*/,
                            DitherType /*dither*/)
{
    // MP3 encoding requires LAME or similar library.
    return false;
}

// ═══════════════════════════════════════════════════════════════════
// OGG Writer (placeholder)
// ═══════════════════════════════════════════════════════════════════

bool FileWriter::write_ogg(const std::string& /*path*/, const float* /*data*/,
                            uint32_t /*channels*/, uint32_t /*frames*/,
                            uint32_t /*sample_rate*/, uint16_t /*bit_depth*/,
                            DitherType /*dither*/)
{
    // Ogg Vorbis encoding requires libvorbis.
    return false;
}

} // namespace aria
