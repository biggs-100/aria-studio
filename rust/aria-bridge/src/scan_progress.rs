// ARIA DAW — Scan Progress Types
// FFI-compatible types for scanner progress reporting.
// These must match the C structs in aria_rust.h exactly.
// The #[repr(C)] attribute ensures C-compatible layout.

use std::os::raw::c_char;

/// FFI result of a scanned file, reported via ScanProgressCallback.
/// Must match `ScannedFileResult` in aria_rust.h.
#[repr(C)]
pub struct FfiScannedFileResult {
    pub path: [c_char; 1024],
    pub file_size: i64,
    pub file_hash: [c_char; 65],
    pub sample_rate: i32,
    pub bit_depth: i32,
    pub channels: i32,
    pub duration_ms: i32,
    pub loudness_lufs: f64,
    pub peak_db: f64,
    pub status: [c_char; 16],
    pub error: [c_char; 256],
}

/// ScanProgressCallback type — matches the C typedef in aria_rust.h.
pub type FfiScanProgressCallback = Option<
    unsafe extern "C" fn(
        indexed: i64,
        total: i64,
        last_file: *const FfiScannedFileResult,
        userdata: *mut std::ffi::c_void,
    ),
>;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ffi_scanned_file_result_repr_c() {
        // Verify the struct has C-compatible layout (not a unit struct)
        let result = FfiScannedFileResult {
            path: [0; 1024],
            file_size: 0,
            file_hash: [0; 65],
            sample_rate: 0,
            bit_depth: 0,
            channels: 0,
            duration_ms: 0,
            loudness_lufs: 0.0,
            peak_db: 0.0,
            status: [0; 16],
            error: [0; 256],
        };
        assert_eq!(result.file_size, 0);
        assert_eq!(result.sample_rate, 0);
    }

    #[test]
    fn test_ffi_progress_callback_is_option() {
        let none: FfiScanProgressCallback = None;
        assert!(none.is_none());
    }
}
