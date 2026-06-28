#ifndef ARIA_FILE_WRITER_H
#define ARIA_FILE_WRITER_H

#include <cstdint>
#include <string>

#include "dither.h"

namespace aria {

/// Audio export format identifiers.
enum class ExportFormat {
    WAV,  ///< RIFF WAV (PCM / float)
    AIFF, ///< AIFF / AIFC
    FLAC, ///< FLAC (via libFLAC if available, else raw)
    MP3,  ///< MP3 (placeholder — requires external library)
    OGG   ///< Ogg Vorbis (placeholder — requires external library)
};

/// Normalization mode for export.
enum class NormalizeMode {
    None,  ///< No normalization.
    Peak,  ///< Normalize to peak level.
    LUFS   ///< Normalize to integrated loudness (EBU R128).
};

/// Static audio file writer.
///
/// Writes interleaved float sample data to common audio file formats.
/// All public methods are static — call FileWriter::write_wav(...) directly.
///
/// The float data should be in the range [-1.0, 1.0] for integer formats.
/// Data is automatically converted to the target bit depth and dithered
/// if a dither type is specified.
class FileWriter {
public:
    /// Write a WAV file (RIFF/PCM or RF64 for >4GB).
    /// Supports: 16, 24, 32-bit integer PCM, and 32-bit float.
    /// @param path         Output file path.
    /// @param data         Interleaved float samples.
    /// @param channels     Number of channels.
    /// @param frames       Number of frames per channel.
    /// @param sample_rate  Sample rate in Hz.
    /// @param bit_depth    Bit depth (16, 24, 32, or 0 for float).
    /// @param dither       Dither type for integer conversion.
    /// @return true on success.
    static bool write_wav(const std::string& path, const float* data,
                          uint32_t channels, uint32_t frames,
                          uint32_t sample_rate, uint16_t bit_depth,
                          DitherType dither = DitherType::None);

    /// Write an AIFF/AIFC file.
    /// @param path         Output file path.
    /// @param data         Interleaved float samples.
    /// @param channels     Number of channels.
    /// @param frames       Number of frames per channel.
    /// @param sample_rate  Sample rate in Hz.
    /// @param bit_depth    Bit depth (16, 24, or 32).
    /// @param dither       Dither type for integer conversion.
    /// @return true on success.
    static bool write_aiff(const std::string& path, const float* data,
                           uint32_t channels, uint32_t frames,
                           uint32_t sample_rate, uint16_t bit_depth,
                           DitherType dither = DitherType::None);

    /// Write a FLAC file.
    /// @param path         Output file path.
    /// @param data         Interleaved float samples.
    /// @param channels     Number of channels.
    /// @param frames       Number of frames per channel.
    /// @param sample_rate  Sample rate in Hz.
    /// @param bit_depth    Bit depth (16 or 24).
    /// @param dither       Dither type for integer conversion.
    /// @return true on success.
    static bool write_flac(const std::string& path, const float* data,
                           uint32_t channels, uint32_t frames,
                           uint32_t sample_rate, uint16_t bit_depth,
                           DitherType dither = DitherType::None);

    /// Write an MP3 file (placeholder).
    static bool write_mp3(const std::string& path, const float* data,
                          uint32_t channels, uint32_t frames,
                          uint32_t sample_rate, uint16_t bit_depth,
                          DitherType dither = DitherType::None);

    /// Write an Ogg Vorbis file (placeholder).
    static bool write_ogg(const std::string& path, const float* data,
                          uint32_t channels, uint32_t frames,
                          uint32_t sample_rate, uint16_t bit_depth,
                          DitherType dither = DitherType::None);

private:
    /// Convert float samples to integer with optional dither.
    static void float_to_int(const float* src, uint8_t* dst,
                             uint32_t count, uint16_t bit_depth,
                             DitherType dither);

    /// Write little-endian value to buffer.
    static void write_le16(uint8_t* buf, uint16_t v);
    static void write_le24(uint8_t* buf, uint32_t v);
    static void write_le32(uint8_t* buf, uint32_t v);
    static void write_be16(uint8_t* buf, uint16_t v);
    static void write_be32(uint8_t* buf, uint32_t v);
};

} // namespace aria

#endif // ARIA_FILE_WRITER_H
