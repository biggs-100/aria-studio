// ARIA DAW — Networking Module
// Collaboration, sync, and cloud services.

pub mod sync;
pub mod protocol;

pub struct Networking {
    enabled: bool,
}

impl Networking {
    pub fn new() -> Self {
        Networking { enabled: false }
    }

    pub fn enable(&mut self) {
        self.enabled = true;
    }

    pub fn disable(&mut self) {
        self.enabled = false;
    }

    pub fn is_enabled(&self) -> bool {
        self.enabled
    }
}
