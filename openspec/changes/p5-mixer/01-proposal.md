# SDD Change: P5 - Mixer

## Goal
Implement the mixing console: channel strips, summing, routing, FX, EQ, and master output.

## Scope
1. Channel strip — volume fader, pan, mute, solo, phase invert, width
2. Mixer engine — 64-bit float summing, group buses, VCA faders
3. Routing — track→bus→master, sends (pre/post), returns, sidechain
4. Built-in EQ — 8-band parametric, zero-latency
5. Master channel — limiter, dither, output config
6. FX rack — plugin chain, dry/wet, bypass

## Reference
- `09_Mixer.md` — Full mixer specification
