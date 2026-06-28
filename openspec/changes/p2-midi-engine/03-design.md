# P2 - MIDI Engine: Design

Key decisions per `05_MIDI_Engine.md`:
1. MidiDriver abstract class with platform-specific implementations
2. MidiGraph as DAG of MIDI processing nodes (similar to AudioGraph)
3. MidiClock with master/slave modes, 24 PPQN
4. NoteTracker manages active notes (including sustain pedal)
5. MPE zones: channel 0 = lower, 1 = master, 2-15 = per-note
6. JitterCompensator records per-device latency calibration
