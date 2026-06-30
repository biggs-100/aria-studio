// ARIA DAW — File System FFI Bridge
// C ABI exports for scanner and watcher operations.
// All functions are `#[no_mangle] extern "C"` with C-compatible types.

use std::ffi::CStr;
use std::os::raw::c_char;

use aria_filesystem::scanner::Scanner as RustScanner;
use aria_filesystem::watcher::{FileWatcher as RustWatcher, WatchEvent as RustWatchEvent};

use crate::scan_progress::{FfiScanProgressCallback, FfiScannedFileResult};

/// WatchEventCallback type — matches the C typedef in aria_rust.h.
pub type FfiWatchEventCallback = Option<
    unsafe extern "C" fn(
        event: *const FfiWatchEvent,
        userdata: *mut std::ffi::c_void,
    ),
>;

/// FFI watch event — matches WatchEvent in aria_rust.h.
#[repr(C)]
pub struct FfiWatchEvent {
    pub event_type: i32,
    pub path: [c_char; 1024],
    pub old_path: [c_char; 1024],
}

// ── Helper: Thread-safe callback data ────────────────────────

/// Stores a raw pointer as `usize` which is `Send`. Safety: the
/// caller must ensure the pointer stays valid for the duration of
/// the operation.
struct SendUsize(usize);
unsafe impl Send for SendUsize {}

/// Conversions between pointer and usize.
impl From<*mut std::ffi::c_void> for SendUsize {
    fn from(p: *mut std::ffi::c_void) -> Self {
        SendUsize(p as usize)
    }
}
impl SendUsize {
    fn as_ptr(&self) -> *mut std::ffi::c_void {
        self.0 as *mut std::ffi::c_void
    }
}

// ── Scanner FFI ──────────────────────────────────────────────

/// Create a file scanner from an array of C string paths.
#[no_mangle]
pub extern "C" fn aria_fs_scanner_create(
    paths: *const *const c_char,
    count: i32,
) -> *mut std::ffi::c_void {
    if paths.is_null() || count <= 0 {
        return std::ptr::null_mut();
    }

    let mut scanner = RustScanner::new();
    let paths_slice = unsafe { std::slice::from_raw_parts(paths, count as usize) };

    for &path_ptr in paths_slice {
        if path_ptr.is_null() {
            continue;
        }
        let path_str = unsafe { CStr::from_ptr(path_ptr) }
            .to_str()
            .unwrap_or("")
            .to_string();
        if !path_str.is_empty() {
            scanner.add_path(&path_str);
        }
    }

    let boxed = Box::new(scanner);
    Box::into_raw(boxed) as *mut std::ffi::c_void
}

/// Run the scanner with an optional progress callback.
#[no_mangle]
pub extern "C" fn aria_fs_scanner_run(
    scanner: *mut std::ffi::c_void,
    progress_cb: FfiScanProgressCallback,
    ud: *mut std::ffi::c_void,
) {
    if scanner.is_null() {
        return;
    }

    let scanner = unsafe { &mut *(scanner as *mut RustScanner) };

    if let Some(cb) = progress_cb {
        let ud = SendUsize::from(ud);
        scanner.set_progress_callback(Box::new(move |indexed, total, file_info| {
            let mut ffi_result = FfiScannedFileResult {
                path: [0; 1024],
                file_size: file_info.file_size as i64,
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

            let path_bytes = file_info.path.as_bytes();
            let path_len = path_bytes.len().min(1023);
            let dest = &mut ffi_result.path;
            for (i, &b) in path_bytes[..path_len].iter().enumerate() {
                dest[i] = b as c_char;
            }
            dest[path_len] = 0;

            let status = if file_info.is_audio() || file_info.is_plugin()
                || file_info.is_project() || file_info.is_preset()
            {
                "indexed"
            } else {
                "skipped"
            };
            let status_bytes = status.as_bytes();
            let status_len = status_bytes.len().min(15);
            let status_dest = &mut ffi_result.status;
            for (i, &b) in status_bytes[..status_len].iter().enumerate() {
                status_dest[i] = b as c_char;
            }
            status_dest[status_len] = 0;

            unsafe {
                cb(indexed as i64, total as i64, &ffi_result, ud.as_ptr());
            }
        }));
    }

    scanner.scan();
}

/// Destroy a scanner and free its resources.
#[no_mangle]
pub extern "C" fn aria_fs_scanner_destroy(scanner: *mut std::ffi::c_void) {
    if !scanner.is_null() {
        unsafe {
            drop(Box::from_raw(scanner as *mut RustScanner));
        }
    }
}

// ── Watcher FFI ──────────────────────────────────────────────

/// Create a file watcher from an array of C string paths.
#[no_mangle]
pub extern "C" fn aria_fs_watcher_create(
    paths: *const *const c_char,
    count: i32,
) -> *mut std::ffi::c_void {
    if paths.is_null() || count <= 0 {
        return std::ptr::null_mut();
    }

    let mut watcher = RustWatcher::new();
    let paths_slice = unsafe { std::slice::from_raw_parts(paths, count as usize) };

    for &path_ptr in paths_slice {
        if path_ptr.is_null() {
            continue;
        }
        let path_str = unsafe { CStr::from_ptr(path_ptr) }
            .to_str()
            .unwrap_or("")
            .to_string();
        if !path_str.is_empty() {
            watcher.add_watch(&path_str);
        }
    }

    let boxed = Box::new(watcher);
    Box::into_raw(boxed) as *mut std::ffi::c_void
}

/// Start watching. Returns true if the watcher started successfully.
#[no_mangle]
pub extern "C" fn aria_fs_watcher_start(
    watcher: *mut std::ffi::c_void,
    event_cb: FfiWatchEventCallback,
    ud: *mut std::ffi::c_void,
) -> bool {
    if watcher.is_null() {
        return false;
    }

    let watcher = unsafe { &mut *(watcher as *mut RustWatcher) };

    if let Some(cb) = event_cb {
        let ud = SendUsize::from(ud);
        watcher.set_event_callback(Box::new(move |event: RustWatchEvent| {
            let mut ffi_event = FfiWatchEvent {
                event_type: match event.kind {
                    aria_filesystem::watcher::WatchEventKind::Created => 0,
                    aria_filesystem::watcher::WatchEventKind::Modified => 1,
                    aria_filesystem::watcher::WatchEventKind::Deleted => 2,
                    aria_filesystem::watcher::WatchEventKind::Renamed => 3,
                },
                path: [0; 1024],
                old_path: [0; 1024],
            };

            let path_bytes = event.path.as_bytes();
            let path_len = path_bytes.len().min(1023);
            let dest = &mut ffi_event.path;
            for (i, &b) in path_bytes[..path_len].iter().enumerate() {
                dest[i] = b as c_char;
            }
            dest[path_len] = 0;

            if let Some(ref old) = event.old_path {
                let old_bytes = old.as_bytes();
                let old_len = old_bytes.len().min(1023);
                let old_dest = &mut ffi_event.old_path;
                for (i, &b) in old_bytes[..old_len].iter().enumerate() {
                    old_dest[i] = b as c_char;
                }
                old_dest[old_len] = 0;
            }

            unsafe {
                cb(&ffi_event, ud.as_ptr());
            }
        }));
    }

    watcher.start().is_ok()
}

/// Stop the watcher.
#[no_mangle]
pub extern "C" fn aria_fs_watcher_stop(watcher: *mut std::ffi::c_void) {
    if !watcher.is_null() {
    let watcher = unsafe { &mut *(watcher as *mut RustWatcher) };
        watcher.stop();
    }
}

/// Destroy a watcher and free its resources.
#[no_mangle]
pub extern "C" fn aria_fs_watcher_destroy(watcher: *mut std::ffi::c_void) {
    if !watcher.is_null() {
        unsafe {
            drop(Box::from_raw(watcher as *mut RustWatcher));
        }
    }
}
