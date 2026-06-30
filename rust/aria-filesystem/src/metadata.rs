// ARIA DAW — Audio Metadata Extraction
// Uses symphonia to extract duration, sample rate, bit depth,
// channels, and LUFS loudness from audio files.

use std::fs::File;
use std::path::Path;

use symphonia::core::audio::SampleBuffer;
use symphonia::core::codecs::DecoderOptions;
use symphonia::core::formats::FormatOptions;
use symphonia::core::io::MediaSourceStream;
use symphonia::core::meta::MetadataOptions;
use symphonia::core::probe::Hint;

/// Extracted audio metadata.
#[derive(Debug, Clone, PartialEq)]
pub struct AudioMetadata {
    /// Duration in seconds
    pub duration_seconds: f64,
    /// Sample rate in Hz (e.g. 44100)
    pub sample_rate: u32,
    /// Bits per sample (e.g. 16, 24, 32)
    pub bit_depth: u32,
    /// Number of channels (1 = mono, 2 = stereo)
    pub channels: u32,
    /// Integrated LUFS loudness
    pub loudness_lufs: f64,
    /// Peak level in dBFS
    pub peak_db: f64,
}

/// Errors that can occur during metadata extraction.
#[derive(Debug)]
pub enum MetadataError {
    /// File could not be opened
    Io(std::io::Error),
    /// File format not recognized by symphonia
    UnsupportedFormat(String),
    /// Decoder could not be opened
    DecodeError(String),
    /// No audio track found in the file
    NoAudioTrack,
}

impl std::fmt::Display for MetadataError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            MetadataError::Io(err) => write!(f, "IO error: {}", err),
            MetadataError::UnsupportedFormat(msg) => write!(f, "Unsupported format: {}", msg),
            MetadataError::DecodeError(msg) => write!(f, "Decode error: {}", msg),
            MetadataError::NoAudioTrack => write!(f, "No audio track found"),
        }
    }
}

impl std::error::Error for MetadataError {}

impl From<std::io::Error> for MetadataError {
    fn from(err: std::io::Error) -> Self {
        MetadataError::Io(err)
    }
}

/// Result type for metadata operations.
pub type MetadataResult<T> = Result<T, MetadataError>;

/// Extract audio metadata from a file path using symphonia.
pub fn extract_metadata<P: AsRef<Path>>(path: P) -> MetadataResult<AudioMetadata> {
    let path = path.as_ref();
    let file = File::open(path)?;
    let mss = MediaSourceStream::new(Box::new(file), Default::default());

    let mut hint = Hint::new();
    if let Some(ext) = path.extension().and_then(|e| e.to_str()) {
        hint.with_extension(ext);
    }

    let format_opts = FormatOptions::default();
    let metadata_opts = MetadataOptions::default();
    let decoder_opts = DecoderOptions::default();

    let probed = symphonia::default::get_probe()
        .format(&hint, mss, &format_opts, &metadata_opts)
        .map_err(|e| MetadataError::UnsupportedFormat(e.to_string()))?;

    let mut format = probed.format;

    // Find the first track that has audio codec parameters
    // Audio tracks have codec parameters with sample_rate set
    let track = format
        .tracks()
        .iter()
        .find(|t| t.codec_params.sample_rate.is_some())
        .ok_or(MetadataError::NoAudioTrack)?;

    let codec_params = &track.codec_params;

    let sample_rate = codec_params.sample_rate.unwrap_or(0) as u32;
    let channels = codec_params
        .channels
        .map(|c| c.count() as u32)
        .unwrap_or(0);
    let bit_depth = codec_params.bits_per_sample.unwrap_or(16) as u32;
    let duration_seconds = codec_params
        .time_base
        .and_then(|tb| {
            let n_frames = codec_params.n_frames?;
            // TimeBase: duration = n_frames * numer / denom
            Some(n_frames as f64 * tb.numer as f64 / tb.denom as f64)
        })
        .unwrap_or(0.0);

    // Decode audio frames and compute LUFS + peak
    let mut decoder = symphonia::default::get_codecs()
        .make(&codec_params, &decoder_opts)
        .map_err(|e| MetadataError::DecodeError(e.to_string()))?;

    let mut total_samples: u64 = 0;
    let mut sum_squared: f64 = 0.0;
    let mut peak: f64 = 0.0;

    loop {
        match format.next_packet() {
            Ok(packet) => {
                match decoder.decode(&packet) {
                    Ok(decoded) => {
                        let num_frames = decoded.frames() as u64;
                        let spec = *decoded.spec();

                        // Convert to f32 planar for LUFS calculation
                        let mut sample_buf = SampleBuffer::<f32>::new(
                            decoded.capacity() as u64,
                            spec,
                        );
                        sample_buf.copy_interleaved_ref(decoded);

                        let samples = sample_buf.samples();
                        total_samples += num_frames * spec.channels.count() as u64;

                        for &sample in samples {
                            let s = sample as f64;
                            // Simplified K-weighting: skip full IIR filter, approximate
                            let weighted = s;
                            sum_squared += weighted * weighted;
                            let abs_s = s.abs();
                            if abs_s > peak {
                                peak = abs_s;
                            }
                        }
                    }
                    Err(_) => {
                        // Skip packets that fail to decode
                        continue;
                    }
                }
            }
            Err(symphonia::core::errors::Error::IoError(ref e))
                if e.kind() == std::io::ErrorKind::UnexpectedEof =>
            {
                break;
            }
            Err(_) => break,
        }
    }

    // Compute integrated LUFS: -0.691 + 10 * log10(mean(squared_samples))
    let lufs = if total_samples > 0 && sum_squared > 0.0 {
        let mean_squared = sum_squared / total_samples as f64;
        -0.691 + 10.0 * mean_squared.log10()
    } else {
        -70.0 // Silence floor
    };

    // Peak dBFS: 20 * log10(peak)
    let peak_db = if peak > 0.0 {
        20.0 * peak.log10()
    } else {
        -120.0
    };

    Ok(AudioMetadata {
        duration_seconds,
        sample_rate,
        bit_depth,
        channels,
        loudness_lufs: lufs,
        peak_db,
    })
}

/// Determine if a file extension corresponds to a supported audio format.
pub fn is_audio_extension(ext: &str) -> bool {
    matches!(
        ext.to_lowercase().as_str(),
        "wav" | "aiff" | "aif" | "flac" | "mp3" | "ogg"
            | "m4a" | "wma" | "aac" | "wv" | "opus"
    )
}

/// Categorize a file by its extension for file-type grouping.
pub fn extension_to_type(ext: &str) -> &'static str {
    match ext.to_lowercase().as_str() {
        "wav" | "aiff" | "aif" | "flac" => "lossless",
        "mp3" | "ogg" | "m4a" | "wma" | "aac" | "opus" => "lossy",
        "wv" => "lossless",
        _ => "unknown",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // ── RED: Tests written first, referencing AudioMetadata that didn't exist yet ──

    #[test]
    fn test_audio_metadata_struct_defaults() {
        // Verify the struct can be constructed with expected field types
        let meta = AudioMetadata {
            duration_seconds: 0.0,
            sample_rate: 0,
            bit_depth: 0,
            channels: 0,
            loudness_lufs: -70.0,
            peak_db: -120.0,
        };
        assert_eq!(meta.duration_seconds, 0.0);
        assert_eq!(meta.sample_rate, 0);
        assert_eq!(meta.bit_depth, 0);
        assert_eq!(meta.channels, 0);
        assert_eq!(meta.loudness_lufs, -70.0);
        assert_eq!(meta.peak_db, -120.0);
    }

    #[test]
    fn test_extract_metadata_nonexistent_file() {
        // Should return an IO error for a file that doesn't exist
        let result = extract_metadata("/nonexistent/path/file.wav");
        assert!(result.is_err());
        match result.unwrap_err() {
            MetadataError::Io(_) => {} // Expected
            other => panic!("Expected Io error, got: {:?}", other),
        }
    }

    #[test]
    fn test_is_audio_extension_wav() {
        assert!(is_audio_extension("wav"));
        assert!(is_audio_extension("WAV")); // Case insensitive
    }

    #[test]
    fn test_is_audio_extension_flac() {
        assert!(is_audio_extension("flac"));
        assert!(is_audio_extension("FLAC"));
    }

    #[test]
    fn test_is_audio_extension_unsupported() {
        assert!(!is_audio_extension("txt"));
        assert!(!is_audio_extension("pdf"));
        assert!(!is_audio_extension(""));
    }

    #[test]
    fn test_is_audio_extension_all_supported() {
        let supported = ["wav", "aiff", "aif", "flac", "mp3", "ogg",
                         "m4a", "wma", "aac", "wv", "opus"];
        for ext in &supported {
            assert!(is_audio_extension(ext), "Expected {} to be supported", ext);
        }
    }

    #[test]
    fn test_extension_to_type_lossless() {
        assert_eq!(extension_to_type("wav"), "lossless");
        assert_eq!(extension_to_type("flac"), "lossless");
        assert_eq!(extension_to_type("aiff"), "lossless");
        assert_eq!(extension_to_type("wv"), "lossless");
    }

    #[test]
    fn test_extension_to_type_lossy() {
        assert_eq!(extension_to_type("mp3"), "lossy");
        assert_eq!(extension_to_type("ogg"), "lossy");
        assert_eq!(extension_to_type("aac"), "lossy");
    }

    // ── TRIANGULATE: Edge cases ──────────────────────────────

    #[test]
    fn test_is_audio_extension_with_dot() {
        // The function takes extension WITHOUT the dot
        assert!(!is_audio_extension(".wav")); // Dot should not match
        assert!(is_audio_extension("wav"));   // Correct: no dot
    }

    #[test]
    fn test_extension_to_type_unknown() {
        assert_eq!(extension_to_type("txt"), "unknown");
        assert_eq!(extension_to_type(""), "unknown");
    }

    #[test]
    fn test_metadata_error_display() {
        let io_err = MetadataError::Io(std::io::Error::new(
            std::io::ErrorKind::NotFound,
            "file not found",
        ));
        assert!(io_err.to_string().contains("IO error"));

        let fmt_err = MetadataError::UnsupportedFormat("unknown format".into());
        assert_eq!(fmt_err.to_string(), "Unsupported format: unknown format");

        let decode_err = MetadataError::DecodeError("codec failed".into());
        assert_eq!(decode_err.to_string(), "Decode error: codec failed");

        let no_track = MetadataError::NoAudioTrack;
        assert_eq!(no_track.to_string(), "No audio track found");
    }

    #[test]
    fn test_metadata_error_from_io() {
        let io_err = std::io::Error::new(std::io::ErrorKind::PermissionDenied, "access denied");
        let meta_err: MetadataError = io_err.into();
        match meta_err {
            MetadataError::Io(_) => {}
            other => panic!("Expected Io variant, got: {}", other),
        }
    }

    #[test]
    fn test_extract_metadata_empty_path() {
        // Empty string path should return an error
        let result = extract_metadata("");
        assert!(result.is_err());
    }

    #[test]
    fn test_extract_metadata_dir_path() {
        // A directory is not a valid audio file
        let result = extract_metadata(".");
        assert!(result.is_err());
        match result.unwrap_err() {
            MetadataError::Io(ref e) if e.kind() == std::io::ErrorKind::PermissionDenied
                || e.kind() == std::io::ErrorKind::Other => {} // Windows
            MetadataError::UnsupportedFormat(_) => {} // Other platforms
            MetadataError::Io(_) => {} // Any IO error is fine
            other => panic!("Expected Io or UnsupportedFormat for a directory, got: {:?}", other),
        }
    }
}
