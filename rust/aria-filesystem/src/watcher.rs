// ARIA DAW — File Watcher
// Monitors file system for changes using the `notify` crate
// with cross-platform support (inotify/FSEvents/ReadDirectoryChangesW).
// Events are debounced to coalesce rapid file changes.

use std::collections::HashMap;
use std::path::Path;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use crossbeam_channel::{unbounded, RecvTimeoutError, TryRecvError};
use notify::{Config, Event, EventKind, RecommendedWatcher, RecursiveMode, Watcher as NotifyWatcher};

/// Watched folder entry.
#[derive(Debug, Clone)]
pub struct WatchedPath {
    pub path: String,
    pub recursive: bool,
}

/// Types of file system events.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WatchEventKind {
    Created,
    Modified,
    Deleted,
    Renamed,
}

/// A file system watch event.
#[derive(Debug, Clone)]
pub struct WatchEvent {
    pub kind: WatchEventKind,
    pub path: String,
    pub old_path: Option<String>,
}

impl WatchEvent {
    pub fn new(kind: WatchEventKind, path: &str) -> Self {
        WatchEvent {
            kind,
            path: path.to_string(),
            old_path: None,
        }
    }

    pub fn renamed(old: &str, new: &str) -> Self {
        WatchEvent {
            kind: WatchEventKind::Renamed,
            path: new.to_string(),
            old_path: Some(old.to_string()),
        }
    }
}

/// Type for event callbacks shared with the background thread.
type SharedCallback = Arc<Mutex<Box<dyn Fn(WatchEvent) + Send + 'static>>>;

/// File system watcher using the `notify` crate.
pub struct FileWatcher {
    watched_paths: Vec<WatchedPath>,
    running: Arc<AtomicBool>,
    debounce_ms: u64,
    event_callback: Option<SharedCallback>,
    debounce_tick_ms: u64,
}

impl FileWatcher {
    /// Create a new file watcher.
    pub fn new() -> Self {
        FileWatcher {
            watched_paths: Vec::new(),
            running: Arc::new(AtomicBool::new(false)),
            debounce_ms: 500,
            event_callback: None,
            debounce_tick_ms: 50,
        }
    }

    /// Set the debounce interval in milliseconds.
    pub fn set_debounce_ms(&mut self, ms: u64) {
        self.debounce_ms = ms;
    }

    /// Set the debounce tick interval.
    pub fn set_debounce_tick_ms(&mut self, ms: u64) {
        self.debounce_tick_ms = ms;
    }

    /// Set a callback invoked for each debounced watch event.
    pub fn set_event_callback(&mut self, cb: Box<dyn Fn(WatchEvent) + Send + 'static>) {
        self.event_callback = Some(Arc::new(Mutex::new(cb)));
    }

    /// Add a path to watch.
    pub fn add_watch(&mut self, path: &str) {
        self.watched_paths.push(WatchedPath {
            path: path.to_string(),
            recursive: true,
        });
    }

    /// Remove a path from the watch list.
    pub fn remove_watch(&mut self, path: &str) {
        self.watched_paths.retain(|p| p.path != path);
    }

    /// Get the list of watched paths.
    pub fn watched_paths(&self) -> &[WatchedPath] {
        &self.watched_paths
    }

    /// Start watching for file changes.
    /// Spawns a background thread for notify and debounce processing.
    pub fn start(&self) -> Result<(), String> {
        if self.watched_paths.is_empty() {
            return Err("No paths configured for watching".to_string());
        }

        let running = self.running.clone();
        if running.load(Ordering::SeqCst) {
            return Err("Watcher is already running".to_string());
        }

        let (raw_tx, raw_rx) = unbounded::<notify::Event>();
        let debounce_duration = Duration::from_millis(self.debounce_ms);
        let debounce_tick = Duration::from_millis(self.debounce_tick_ms);

        let event_callback: Option<SharedCallback> = self.event_callback.clone();

        // Build notify watcher
        let mut watcher = RecommendedWatcher::new(
            move |res: Result<Event, notify::Error>| {
                if let Ok(event) = res {
                    let _ = raw_tx.send(event);
                }
            },
            Config::default(),
        )
        .map_err(|e| format!("Failed to create watcher: {}", e))?;

        // Register watched paths
        let paths = self.watched_paths.clone();
        for wp in &paths {
            let mode = if wp.recursive {
                RecursiveMode::Recursive
            } else {
                RecursiveMode::NonRecursive
            };
            watcher
                .watch(Path::new(&wp.path), mode)
                .map_err(|e| format!("Failed to watch path '{}': {}", wp.path, e))?;
        }

        running.store(true, Ordering::SeqCst);

        // Background thread: debounce loop
        let running_clone = running.clone();
        thread::spawn(move || {
            let _watcher = watcher;
            let mut pending: Vec<(WatchEventKind, String)> = Vec::new();
            let mut last_event_time: Option<std::time::Instant> = None;

            loop {
                if !running_clone.load(Ordering::SeqCst) {
                    break;
                }

                // Wait for next event (with timeout for responsiveness)
                let got_new = match raw_rx.recv_timeout(debounce_tick) {
                    Ok(event) => {
                        if let Some(we) = notify_event_to_watch_event(&event) {
                            pending.push((we.kind, we.path));
                        }
                        true
                    }
                    Err(RecvTimeoutError::Timeout) => false,
                    Err(RecvTimeoutError::Disconnected) => {
                        running_clone.store(false, Ordering::SeqCst);
                        return;
                    }
                };

                // Drain any remaining events (non-blocking)
                loop {
                    match raw_rx.try_recv() {
                        Ok(event) => {
                            if let Some(we) = notify_event_to_watch_event(&event) {
                                pending.push((we.kind, we.path));
                            }
                        }
                        Err(TryRecvError::Empty) => break,
                        Err(TryRecvError::Disconnected) => {
                            running_clone.store(false, Ordering::SeqCst);
                            return;
                        }
                    }
                }

                if got_new {
                    last_event_time = Some(std::time::Instant::now());
                }

                // Flush pending events if debounce window has elapsed
                if !pending.is_empty() {
                    if let Some(last) = last_event_time {
                        if last.elapsed() >= debounce_duration {
                            let batch = drain_deduplicated(&mut pending);
                            if let Some(ref cb) = event_callback {
                                if let Ok(cb) = cb.lock() {
                                    for (kind, path) in batch {
                                        cb(WatchEvent::new(kind, &path));
                                    }
                                }
                            }
                            pending.clear();
                            last_event_time = None;
                        }
                    }
                }
            }

            // Final flush on stop
            let batch = drain_deduplicated(&mut pending);
            if let Some(ref cb) = event_callback {
                if let Ok(cb) = cb.lock() {
                    for (kind, path) in batch {
                        cb(WatchEvent::new(kind, &path));
                    }
                }
            }
        });

        Ok(())
    }

    /// Stop watching.
    pub fn stop(&self) {
        self.running.store(false, Ordering::SeqCst);
    }

    /// Check if the watcher is running.
    pub fn is_running(&self) -> bool {
        self.running.load(Ordering::SeqCst)
    }

    /// Determine if the platform supports native file watching.
    pub fn platform_supports_watching() -> bool {
        cfg!(any(
            target_os = "linux",
            target_os = "macos",
            target_os = "windows"
        ))
    }
}

/// Convert a notify `Event` into a `WatchEvent`.
fn notify_event_to_watch_event(event: &notify::Event) -> Option<WatchEvent> {
    let kind = match event.kind {
        EventKind::Create(_) => WatchEventKind::Created,
        EventKind::Modify(_) => WatchEventKind::Modified,
        EventKind::Remove(_) => WatchEventKind::Deleted,
        EventKind::Any => WatchEventKind::Modified,
        _ => return None,
    };

    let path = event.paths.first()?;
    Some(WatchEvent::new(kind, &path.to_string_lossy()))
}

/// Drain pending events, keeping only the LATEST event per path.
fn drain_deduplicated(pending: &mut Vec<(WatchEventKind, String)>) -> Vec<(WatchEventKind, String)> {
    let mut seen: HashMap<String, WatchEventKind> = HashMap::new();
    for (kind, path) in pending.drain(..) {
        seen.insert(path, kind);
    }
    // Convert back from HashMap (String→WatchEventKind) to Vec<(WatchEventKind, String)>
    seen.into_iter().map(|(path, kind)| (kind, path)).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_watcher_new_is_stopped() {
        let watcher = FileWatcher::new();
        assert!(!watcher.is_running());
        assert!(watcher.watched_paths().is_empty());
    }

    #[test]
    fn test_watcher_add_remove_path() {
        let mut watcher = FileWatcher::new();
        watcher.add_watch("/some/path");
        assert_eq!(watcher.watched_paths().len(), 1);
        assert_eq!(watcher.watched_paths()[0].path, "/some/path");

        watcher.add_watch("/other/path");
        assert_eq!(watcher.watched_paths().len(), 2);

        watcher.remove_watch("/some/path");
        assert_eq!(watcher.watched_paths().len(), 1);
        assert_eq!(watcher.watched_paths()[0].path, "/other/path");
    }

    #[test]
    fn test_watcher_start_no_paths() {
        let watcher = FileWatcher::new();
        let result = watcher.start();
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("No paths configured"));
    }

    #[test]
    fn test_watcher_start_stop() {
        let mut watcher = FileWatcher::new();
        watcher.add_watch(".");

        let result = watcher.start();
        assert!(result.is_ok(), "Start should succeed: {:?}", result);
        assert!(watcher.is_running());

        watcher.stop();
        std::thread::sleep(Duration::from_millis(150));
        assert!(!watcher.is_running());
    }

    #[test]
    fn test_watch_event_creation() {
        let event = WatchEvent::new(WatchEventKind::Created, "/sounds/kick.wav");
        assert_eq!(event.kind, WatchEventKind::Created);
        assert_eq!(event.path, "/sounds/kick.wav");
        assert!(event.old_path.is_none());
    }

    #[test]
    fn test_watch_event_renamed() {
        let event = WatchEvent::renamed("/sounds/old.wav", "/sounds/new.wav");
        assert_eq!(event.kind, WatchEventKind::Renamed);
        assert_eq!(event.path, "/sounds/new.wav");
        assert_eq!(event.old_path.as_deref(), Some("/sounds/old.wav"));
    }

    #[test]
    fn test_watch_event_kind_equality() {
        assert_eq!(WatchEventKind::Created, WatchEventKind::Created);
        assert_ne!(WatchEventKind::Created, WatchEventKind::Deleted);
        assert_ne!(WatchEventKind::Modified, WatchEventKind::Renamed);
    }

    #[test]
    fn test_platform_supports_watching() {
        assert!(FileWatcher::platform_supports_watching());
    }

    #[test]
    fn test_watcher_debounce_config() {
        let mut watcher = FileWatcher::new();
        assert_eq!(watcher.debounce_ms, 500);

        watcher.set_debounce_ms(1000);
        assert_eq!(watcher.debounce_ms, 1000);

        watcher.set_debounce_tick_ms(25);
        assert_eq!(watcher.debounce_tick_ms, 25);
    }

    #[test]
    fn test_drain_deduplicated() {
        let mut events = vec![
            (WatchEventKind::Created, "/a.wav".to_string()),
            (WatchEventKind::Modified, "/a.wav".to_string()),
            (WatchEventKind::Created, "/b.wav".to_string()),
        ];

        let result = drain_deduplicated(&mut events);
        assert!(events.is_empty());

        assert_eq!(result.len(), 2, "Should have 2 unique paths");

        let a_event = result.iter().find(|(_, p)| p == "/a.wav");
        assert!(a_event.is_some());
        assert_eq!(a_event.unwrap().0, WatchEventKind::Modified);

        let b_event = result.iter().find(|(_, p)| p == "/b.wav");
        assert!(b_event.is_some());
        assert_eq!(b_event.unwrap().0, WatchEventKind::Created);
    }

    #[test]
    fn test_drain_deduplicated_empty() {
        let mut events = vec![];
        let result = drain_deduplicated(&mut events);
        assert!(result.is_empty());
    }

    #[test]
    fn test_watcher_add_multiple_remove_nonexistent() {
        let mut watcher = FileWatcher::new();
        watcher.add_watch("/path/one");
        watcher.add_watch("/path/two");
        watcher.add_watch("/path/three");

        watcher.remove_watch("/path/nonexistent");
        assert_eq!(watcher.watched_paths().len(), 3);

        watcher.remove_watch("/path/two");
        assert_eq!(watcher.watched_paths().len(), 2);
    }
}
