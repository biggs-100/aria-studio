// ARIA DAW — File Scanner
// Recursively scans directories for supported files using walkdir,
// computes SHA-256 content hashes, and reports progress via callback.

use std::fs::File;
use std::io::Read;
use std::path::Path;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use sha2::{Digest, Sha256};
use walkdir::WalkDir;

use crate::FileInfo;

/// Callback type for scan progress. Receives indexed count, total count,
/// and the most recently scanned file.
/// Note: the scanner runs synchronously; the callback is called from the
/// same thread that calls scan(), so Send is not required unless the
/// callback is shared across threads.
pub type ScanProgressFn = Box<dyn Fn(u64, u64, &FileInfo)>;

/// The file scanner.
pub struct Scanner {
    root_paths: Vec<String>,
    recursive: bool,
    cancelled: Arc<AtomicBool>,
    progress_callback: Option<ScanProgressFn>,
}

impl Scanner {
    /// Create a new scanner with no configured paths.
    pub fn new() -> Self {
        Scanner {
            root_paths: Vec::new(),
            recursive: true,
            cancelled: Arc::new(AtomicBool::new(false)),
            progress_callback: None,
        }
    }

    /// Add a root path to scan.
    pub fn add_path(&mut self, path: &str) {
        self.root_paths.push(path.to_string());
    }

    /// Set whether scanning should be recursive.
    pub fn set_recursive(&mut self, recursive: bool) {
        self.recursive = recursive;
    }

    /// Set a progress callback that is invoked for each discovered file.
    /// The callback receives (indexed_count, total_discovered, current_file).
    pub fn set_progress_callback(&mut self, cb: ScanProgressFn) {
        self.progress_callback = Some(cb);
    }

    /// Cancel an in-progress scan. The scan will stop at the next file boundary.
    pub fn cancel(&self) {
        self.cancelled.store(true, Ordering::SeqCst);
    }

    /// Reset the cancellation flag so the scanner can be reused.
    pub fn reset_cancellation(&mut self) {
        self.cancelled.store(false, Ordering::SeqCst);
    }

    /// Get a clone of the cancellation flag (for checking during scan).
    pub fn is_cancelled(&self) -> bool {
        self.cancelled.load(Ordering::SeqCst)
    }

    /// Scan all configured paths and return discovered files.
    /// During scanning, the progress callback (if set) is invoked for each file.
    pub fn scan(&self) -> Vec<FileInfo> {
        let mut results = Vec::new();
        let mut discovered: Vec<FileInfo> = Vec::new();

        // Phase 1: discover all files
        for root in &self.root_paths {
            if self.is_cancelled() {
                break;
            }

            let walker = if self.recursive {
                WalkDir::new(root).follow_links(false)
            } else {
                WalkDir::new(root).max_depth(1).follow_links(false)
            };

            for entry in walker.into_iter().filter_entry(|e| {
                // Skip symlinks
                !e.path_is_symlink()
            }) {
                if self.is_cancelled() {
                    break;
                }

                match entry {
                    Ok(entry) => {
                        if entry.file_type().is_file() {
                            let path = entry.path();
                            if let Some(info) = Self::build_file_info(path) {
                                discovered.push(info);
                            }
                        }
                    }
                    Err(_) => continue,
                }
            }
        }

        let total = discovered.len() as u64;

        // Phase 2: process each file (with progress callback)
        for (i, info) in discovered.into_iter().enumerate() {
            if self.is_cancelled() {
                break;
            }

            if let Some(ref cb) = self.progress_callback {
                cb(i as u64 + 1, total, &info);
            }

            results.push(info);
        }

        results
    }

    /// Scan paths and return discovered files grouped by type.
    pub fn scan_grouped(&self) -> FileGroupedResults {
        let files = self.scan();
        let mut audio = Vec::new();
        let mut plugins = Vec::new();
        let mut projects = Vec::new();
        let mut presets = Vec::new();
        let mut other = Vec::new();

        for f in files {
            if f.is_audio() {
                audio.push(f);
            } else if f.is_plugin() {
                plugins.push(f);
            } else if f.is_project() {
                projects.push(f);
            } else if f.is_preset() {
                presets.push(f);
            } else {
                other.push(f);
            }
        }

        FileGroupedResults {
            audio,
            plugins,
            projects,
            presets,
            other,
        }
    }

    /// Build a FileInfo from a path, returning None if the file can't be read.
    fn build_file_info(path: &Path) -> Option<FileInfo> {
        let metadata = path.metadata().ok()?;
        let file_size = metadata.len();
        let modified_at = metadata
            .modified()
            .ok()
            .and_then(|t| t.duration_since(std::time::UNIX_EPOCH).ok())
            .map(|d| d.as_secs())
            .unwrap_or(0);

        let extension = path
            .extension()
            .and_then(|e| e.to_str())
            .unwrap_or("")
            .to_lowercase();

        Some(FileInfo {
            path: path.to_string_lossy().to_string(),
            file_size,
            modified_at,
            is_directory: false,
            extension,
        })
    }

    /// Compute the SHA-256 hash of a file's contents.
    pub fn hash_file(path: &Path) -> Result<String, std::io::Error> {
        let mut file = File::open(path)?;
        let mut hasher = Sha256::new();
        let mut buffer = [0u8; 8192];

        loop {
            let bytes_read = file.read(&mut buffer)?;
            if bytes_read == 0 {
                break;
            }
            hasher.update(&buffer[..bytes_read]);
        }

        let hash = hasher.finalize();
        Ok(format!("{:x}", hash))
    }
}

/// Results of a grouped scan.
#[derive(Debug, Default)]
pub struct FileGroupedResults {
    pub audio: Vec<FileInfo>,
    pub plugins: Vec<FileInfo>,
    pub projects: Vec<FileInfo>,
    pub presets: Vec<FileInfo>,
    pub other: Vec<FileInfo>,
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::io::Write;
    use tempfile::TempDir;

    // ── RED: Tests written first, referencing Scanner API ─────

    #[test]
    fn test_scanner_new_is_empty() {
        let scanner = Scanner::new();
        assert!(scanner.root_paths.is_empty());
        assert!(scanner.recursive);
        assert!(!scanner.is_cancelled());
    }

    #[test]
    fn test_scanner_add_path() {
        let mut scanner = Scanner::new();
        scanner.add_path("/some/path");
        assert_eq!(scanner.root_paths.len(), 1);
        assert_eq!(scanner.root_paths[0], "/some/path");
    }

    #[test]
    fn test_scan_empty_paths() {
        let scanner = Scanner::new();
        let results = scanner.scan();
        assert_eq!(results.len(), 0, "Scanner with no paths should return empty results");
    }

    #[test]
    fn test_hash_file_known_content() {
        let dir = TempDir::new().unwrap();
        let file_path = dir.path().join("test.txt");
        let mut f = fs::File::create(&file_path).unwrap();
        f.write_all(b"hello world").unwrap();
        drop(f);

        // SHA-256 of "hello world"
        let expected = "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9";
        let hash = Scanner::hash_file(&file_path).unwrap();
        assert_eq!(hash, expected);
    }

    #[test]
    fn test_hash_file_empty() {
        let dir = TempDir::new().unwrap();
        let file_path = dir.path().join("empty.txt");
        fs::File::create(&file_path).unwrap();

        // SHA-256 of empty content
        let expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
        let hash = Scanner::hash_file(&file_path).unwrap();
        assert_eq!(hash, expected);
    }

    #[test]
    fn test_hash_file_nonexistent() {
        let result = Scanner::hash_file(Path::new("/nonexistent/file.wav"));
        assert!(result.is_err());
    }

    #[test]
    fn test_scan_directory_with_files() {
        let dir = TempDir::new().unwrap();

        // Create some test files
        fs::File::create(dir.path().join("kick.wav")).unwrap();
        fs::File::create(dir.path().join("snare.wav")).unwrap();
        fs::File::create(dir.path().join("notes.txt")).unwrap();

        let mut scanner = Scanner::new();
        scanner.add_path(dir.path().to_str().unwrap());

        let results = scanner.scan();
        // Should find 3 files (2 audio + 1 text)
        assert_eq!(results.len(), 3, "Should find all 3 files");
    }

    #[test]
    fn test_scan_directory_extension_detection() {
        let dir = TempDir::new().unwrap();

        fs::File::create(dir.path().join("kick.wav")).unwrap();
        fs::File::create(dir.path().join("lead.vst3")).unwrap();
        fs::File::create(dir.path().join("project.aria")).unwrap();

        let mut scanner = Scanner::new();
        scanner.add_path(dir.path().to_str().unwrap());

        let grouped = scanner.scan_grouped();
        assert_eq!(grouped.audio.len(), 1, "Should find 1 audio file");
        assert_eq!(grouped.plugins.len(), 1, "Should find 1 plugin file");
        assert_eq!(grouped.projects.len(), 1, "Should find 1 project file");
        assert_eq!(grouped.presets.len(), 0, "Should find 0 preset files");
        assert_eq!(grouped.other.len(), 0, "Should find 0 other files");
    }

    // ── TRIANGULATE ──────────────────────────────────────────

    #[test]
    fn test_scan_recursive_finds_nested_files() {
        let dir = TempDir::new().unwrap();
        fs::create_dir(dir.path().join("subdir")).unwrap();
        fs::create_dir(dir.path().join("subdir/deep")).unwrap();

        fs::File::create(dir.path().join("root.wav")).unwrap();
        fs::File::create(dir.path().join("subdir/nested.wav")).unwrap();
        fs::File::create(dir.path().join("subdir/deep/deep.wav")).unwrap();

        let mut scanner = Scanner::new();
        scanner.add_path(dir.path().to_str().unwrap());

        let results = scanner.scan();
        assert_eq!(results.len(), 3, "Recursive scan should find files in subdirectories");

        // Verify all paths are present
        let paths: Vec<&str> = results.iter().map(|f| f.path.as_str()).collect();
        assert!(paths.iter().any(|p| p.ends_with("root.wav")));
        assert!(paths.iter().any(|p| p.ends_with("nested.wav")));
        assert!(paths.iter().any(|p| p.ends_with("deep.wav")));
    }

    #[test]
    fn test_scan_non_recursive() {
        let dir = TempDir::new().unwrap();
        fs::create_dir(dir.path().join("subdir")).unwrap();

        fs::File::create(dir.path().join("root.wav")).unwrap();
        fs::File::create(dir.path().join("subdir/nested.wav")).unwrap();

        let mut scanner = Scanner::new();
        scanner.add_path(dir.path().to_str().unwrap());
        scanner.set_recursive(false);

        let results = scanner.scan();
        assert_eq!(results.len(), 1, "Non-recursive scan should only find root files");
        assert!(results[0].path.ends_with("root.wav"));
    }

    #[test]
    fn test_scan_progress_callback() {
        let dir = TempDir::new().unwrap();
        for i in 0..5 {
            fs::File::create(dir.path().join(format!("file_{}.wav", i))).unwrap();
        }

        let mut scanner = Scanner::new();
        scanner.add_path(dir.path().to_str().unwrap());

        let progress: Arc<std::sync::Mutex<Vec<(u64, u64)>>> = Arc::new(std::sync::Mutex::new(Vec::new()));
        let progress_clone = progress.clone();
        scanner.set_progress_callback(Box::new(move |indexed, total, _info| {
            progress_clone.lock().unwrap().push((indexed, total));
        }));

        let results = scanner.scan();
        assert_eq!(results.len(), 5);

        let progress_data = progress.lock().unwrap();
        assert_eq!(progress_data.len(), 5, "Progress should be called for each file");
        // Verify total is always 5
        for &(_, total) in progress_data.iter() {
            assert_eq!(total, 5, "Total should be 5 for all progress calls");
        }
        // Verify indexed goes from 1..5
        assert_eq!(progress_data[0].0, 1);
        assert_eq!(progress_data[4].0, 5);
    }

    #[test]
    fn test_scan_cancellation() {
        let dir = TempDir::new().unwrap();
        for i in 0..100 {
            fs::File::create(dir.path().join(format!("file_{}.wav", i))).unwrap();
        }

        let mut scanner = Scanner::new();
        scanner.add_path(dir.path().to_str().unwrap());
        scanner.cancel();

        let results = scanner.scan();
        // When cancelled before scan starts, results should be empty
        assert!(results.len() == 0, "Cancelled scan should return early");
    }

    #[test]
    fn test_scan_empty_directory() {
        let dir = TempDir::new().unwrap();
        let mut scanner = Scanner::new();
        scanner.add_path(dir.path().to_str().unwrap());
        let results = scanner.scan();
        assert_eq!(results.len(), 0, "Empty directory should yield no files");
    }

    #[test]
    fn test_nonexistent_directory_scan() {
        let scanner = Scanner::new();
        // No paths configured
        let results = scanner.scan();
        assert_eq!(results.len(), 0);
    }
}
