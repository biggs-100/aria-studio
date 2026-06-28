# SDD Change: P4 - Piano Roll

## Goal
Implement ARIA's flagship feature: a vector-based GPU-accelerated MIDI piano roll editor.

## Scope
1. Note data model — Note, NoteCollection, spatial index
2. GPU rendering — instanced notes, grid, keyboard, viewport culling
3. Tools — pencil, select, paint, erase, cut, glue, ramp
4. Editing — move, resize, transpose, copy/paste, undo
5. Velocity editing — lane, bars, ramp tool
6. Grid & snap — adaptive, scale-aware
7. Expression lanes — pitch bend, CC, pressure, MPE
8. Ghost notes — adjacent clips, scale, reference track
9. Scale & chord tools — 14 scales, chord generator, arpeggiator
10. Quantize & humanize — grid quantize, groove templates
11. MIDI step input — real-time step recording
12. Multi-monitor — independent window

## Slices
- Slice 1: Data model + spatial index + basic rendering
- Slice 2: Tools + editing
- Slice 3: Velocity + expression lanes
- Slice 4: Scale/chord + quantize + ghost notes

## Reference
- `06_Piano_Roll.md` — Full piano roll specification
