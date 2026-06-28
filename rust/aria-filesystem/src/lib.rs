// ARIA DAW — File System Module
// File indexing, watching, and metadata extraction.

pub mod scanner;
pub mod watcher;

/// Supported audio file extensions.
pub const SUPPORTED_AUDIO_EXTENSIONS: &[&str] = &[
    "wav", "aiff", "aif", "flac", "mp3", "ogg",
    "m4a", "wma", "aac", "wv", "opus",
];

pub struct FileInfo {
    pub path: String,
    pub file_size: u64,
    pub modified_at: u64,
    pub is_directory: bool,
    pub extension: String,
}

impl FileInfo {
    pub fn is_audio(&self) -> bool {
        let ext = self.extension.to_lowercase();
        SUPPORTED_AUDIO_EXTENSIONS.contains(&ext.as_str())
    }

    pub fn is_plugin(&self) -> bool {
        matches!(self.extension.to_lowercase().as_str(), "clap" | "vst3" | "vst" | "component" | "lv2")
    }

    pub fn is_project(&self) -> bool {
        self.extension.to_lowercase() == "aria"
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_audio_detection() {
        let info = FileInfo {
            path: "/sounds/kick.wav".into(),
            file_size: 1024,
            modified_at: 0,
            is_directory: false,
            extension: "wav".into(),
        };
        assert!(info.is_audio());
        assert!(!info.is_plugin());
    }
}
