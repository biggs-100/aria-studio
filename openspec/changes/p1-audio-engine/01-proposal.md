# SDD Change: P1 - Audio Engine

## Proposal

### Goal
Implement the complete audio engine: real-time playback and recording with low latency, audio graph processing, buffer management, transport, DSP core, and export.

### Scope
1. **Audio device abstraction** — ASIO (Win), WASAPI (Win), CoreAudio (macOS), ALSA/PipeWire (Linux)
2. **Buffer management** — Lock-free buffer pool, SIMD operations (SSE2/AVX2/NEON)
3. **Audio graph** — DAG processing, node types (input, output, gain, meter), lock-free parameter updates
4. **Transport** — Play/stop/record, sample-accurate clock, tempo map, loop/punch
5. **DSP core** — Biquad filters, SVF, gain/pan/phase utilities, metering (peak/RMS)
6. **Export** — Offline renderer, WAV/AIFF/FLAC output, dithering

### Out of Scope
- MIDI engine (P2)
- Piano roll (P4)
- Plugin host (P6)
- Automation engine (P7)
- Any UI (P10)

### References
- `04_Audio_Engine.md` — Full audio engine spec
- `03_System_Architecture.md` §6 — Threading model
- `10_DSP.md` §2-4 — DSP primitives and SIMD
