# P5 - Mixer: Tasks

## Slice 1: Mixer engine + channel strip + routing
- [x] T-501: Channel strip (volume, pan, mute, solo, phase, width)
- [x] T-502: Mixer + summing (64-bit float, groups, buses)
- [x] T-503: Routing + sends + returns + sidechain

## Slice 2: EQ + master channel + Mixer UI
- [x] T-504: Built-in 8-band EQ (biquad cascade)
- [x] T-505: Master channel (limiter, dither, output)
- [x] T-506: Mixer UI layout (console, channel strip rendering)

## Fix-ups (post-verify)
- [x] F-101: FX Rack dry/wet mix data model + API
- [x] F-102: Sidechain process() implementation with Mixer wiring
- [x] F-103: Send audio tapping wired into Mixer::process()
- [x] F-104: Dither behavioral test (shaped noise verification)
