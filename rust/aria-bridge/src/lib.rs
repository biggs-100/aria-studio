// ARIA DAW — Rust ↔ C FFI Bridge
// Exposes Rust domain crates via #[no_mangle] extern "C" functions.
// Builds as cdylib (shared library) and staticlib (static library).

pub mod db_ffi;
pub mod fs_ffi;
pub mod scan_progress;
