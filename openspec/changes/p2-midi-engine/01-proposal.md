# SDD Change: P2 - MIDI Engine

## Goal
Implement complete MIDI engine: device I/O, data model, MIDI graph, clock sync, recording, and MPE.

## Scope
1. MIDI device abstraction — WinMM (Windows), CoreMIDI (macOS), ALSA (Linux), USB MIDI, virtual ports
2. MIDI data model — MidiEvent, NoteTracker, MidiClip
3. MIDI graph — processing nodes, router, transforms
4. MIDI clock — master/slave, 24 PPQN, sync
5. Recording — timestamped capture, jitter compensation, quantize on record
6. MPE — decoder/encoder, per-note expression

## Out of Scope
- Piano roll editing (P4)
- Automation integration (P7)
- Scripting API (P11)

## References
- `05_MIDI_Engine.md` — Full MIDI engine specification
