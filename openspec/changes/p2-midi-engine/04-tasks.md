# P2 - MIDI Engine: Tasks

## Slice 1: Core MIDI model + graph + clock

**T-201**: ✅ MIDI data model — MidiEvent, MidiNote, MidiClip, NoteTracker
**T-202**: ✅ MIDI graph — MidiGraph DAG, MidiNode base, router node
**T-203**: ✅ MIDI clock — master/slave, 24 PPQN, sync

## Slice 2: Drivers + Recording + MPE

**T-204**: Platform MIDI drivers — WinMM, CoreMIDI, ALSA, virtual ports
**T-205**: MIDI recording — timestamp capture, jitter compensation, quantize
**T-206**: MPE support — decoder, encoder, zone management
