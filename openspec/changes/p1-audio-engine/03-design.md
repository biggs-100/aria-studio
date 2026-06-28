# P1 - Audio Engine: Design

Key design decisions per `04_Audio_Engine.md`:
1. AudioDriver abstract class with platform-specific implementations
2. AudioGraph as DAG processed in topological order per callback
3. LockFreeBufferPool for all audio memory
4. AudioClock sample-accurate with double-buffered tempo map
5. Transport state machine: Stop ↔ Play ↔ Record ↔ Pause
6. AudioDiagnostics for x-run detection and recovery
