// ARIA DAW — Network Protocol
// Protocol definitions for ARIA Link communication.

pub enum MessageType {
    ProjectDelta,
    CursorPosition,
    ChatMessage,
    AudioStream,
    Ping,
}

pub struct ProtocolMessage {
    pub message_type: MessageType,
    pub payload: Vec<u8>,
    pub timestamp: u64,
}

/// Serialize a message to binary format.
pub fn serialize(msg: &ProtocolMessage) -> Vec<u8> {
    Vec::new()
}

/// Deserialize a message from binary format.
pub fn deserialize(data: &[u8]) -> Option<ProtocolMessage> {
    None
}
