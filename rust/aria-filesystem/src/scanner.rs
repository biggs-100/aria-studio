// ARIA DAW — File Scanner
// Recursively scans directories for supported files.

use crate::FileInfo;

pub struct Scanner {
    root_paths: Vec<String>,
    recursive: bool,
}

impl Scanner {
    pub fn new() -> Self {
        Scanner {
            root_paths: Vec::new(),
            recursive: true,
        }
    }

    pub fn add_path(&mut self, path: &str) {
        self.root_paths.push(path.to_string());
    }

    pub fn set_recursive(&mut self, recursive: bool) {
        self.recursive = recursive;
    }

    /// Scan all configured paths and return discovered files.
    /// Placeholder — will use walkdir crate in production.
    pub fn scan(&self) -> Vec<FileInfo> {
        Vec::new()
    }
}
