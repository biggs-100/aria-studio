# Mixer Specification

## Purpose

The Mixer manages channel strips, 64-bit float summing, routing (track→bus→master), sends/returns, sidechain, built-in EQ, FX racks, metering, and master output.

## Requirements

### Requirement: Channel Strip

Every audio channel MUST provide volume fader (−∞ to +24 dB, 0.1 dB resolution), pan (−1.0 to +1.0), mute, solo, phase invert, and stereo width (0.0–2.0). Volume MUST combine base, automation, and VCA contribution.

#### Scenario: Fader adjusts volume
- GIVEN a channel at 0 dB
- WHEN `set_volume(-6.0)` is called
- THEN linear gain reflects −6 dB attenuation

#### Scenario: Solo exclusivity
- GIVEN 3 channels where channel B is soloed
- WHEN the mixer processes
- THEN only channel B and return channels are audible

### Requirement: Mixer Engine

Internal summation MUST use 64-bit float with no intermediate truncation. Supports up to 1024 channels, 128 buses, configurable pan law (equal-power −3 dB default, linear 0 dB, linear −6 dB, stereo balance). Group buses sum assigned channels with parent-child hierarchy: child tracks route into their parent `GroupTrack` bus automatically. VCA tracks control slave fader groups without passing audio; VCA volume contribution combines multiplicatively with each slave's base volume. Group buses MUST support nested hierarchy (group within group).

#### Scenario: 64-bit summing preserves precision
- GIVEN 100 channels summing into one bus at +6 dB each
- WHEN the mixer processes
- THEN the accumulator retains headroom with no clipping

#### Scenario: Group bus sums children with hierarchy
- GIVEN a GroupTrack with two child AudioTracks at −6 dB each
- WHEN the mixer processes the group bus
- THEN the group output is ~0 dB (sum of two −6 dB signals)

#### Scenario: VCA fader applies multiplicative gain to slaves
- GIVEN a VCA track at −6 dB with a slave at −6 dB
- WHEN the mixer processes
- THEN the slave's effective volume is −12 dB (additive in dB = multiplicative in linear)

#### Scenario: Nested group sums sub-group output
- GIVEN Group A containing Group B which has two child AudioTracks
- WHEN all tracks process
- THEN Group B sums children, Group A sums Group B's output with its other children

### Requirement: Routing

Channels MUST route to Master, a Bus, a Track, a Group, or External output. Track→Group routing SHALL be the default for children of a GroupTrack in Summing mode. VCA tracks MUST NOT route audio — they control slave faders only. Sends MUST support pre-fader and post-fader with configurable level. Return channels process send audio to Master. Sidechain MUST route a source channel's audio to a target's sidechain input.

#### Scenario: Post-fader send to return
- GIVEN a channel with a −12 dB post-fader send to Reverb return
- WHEN the mixer processes
- THEN the return receives the post-fader attenuated signal

#### Scenario: Sidechain triggers compressor
- GIVEN Kick routed as sidechain source to Bass
- WHEN Kick hits and exceeds threshold
- THEN Bass compressor detects the sidechain signal and applies gain reduction

#### Scenario: Child track routes to parent group by default
- GIVEN an AudioTrack added as a child of GroupTrack "Drums"
- WHEN the mixer queries the track's route target
- THEN output type is `Group` targeting the parent GroupTrack

#### Scenario: VCA track carries no audio
- GIVEN a VCA track assigned to control 3 slave tracks
- WHEN the mixer processes the VCA track's channel
- THEN no audio samples are produced on the VCA output bus

### Requirement: Built-in EQ

Each channel MUST provide 8-band parametric EQ with zero-latency biquad filters. Band types: LowShelf, HighShelf, Peak, LowCut, HighCut, BandPass, Notch. Frequency: 20–20000 Hz. Gain: −36 to +36 dB. Q: 0.1–10. Bypass passes audio unchanged.

#### Scenario: Band shapes frequency response
- GIVEN a +3 dB Peak band at 1 kHz, Q 1.0
- WHEN 1 kHz audio is processed
- THEN output is ~+3 dB relative to bypass

#### Scenario: Bypass preserves signal
- GIVEN an EQ with active bands
- WHEN `set_bypass(true)` is set and audio processed
- THEN output equals input sample-for-sample

### Requirement: Master Channel

The master MUST provide volume, brickwall limiter (threshold −0.1 to −6 dB, ceiling −0.3 dB, configurable release), dithering (None, Triangular, Shaped), and an FX insert chain. Limiter MUST be enabled by default.

#### Scenario: Limiter enforces ceiling
- GIVEN limiter threshold at −1.0 dB, ceiling −0.3 dB
- WHEN input exceeds −1.0 dBFS
- THEN output stays at or below −0.3 dBFS

#### Scenario: Dither decorrelates noise
- GIVEN master at 16-bit with Triangular dither
- WHEN audio is processed
- THEN quantization noise is decorrelated from signal

### Requirement: FX Rack

Each channel and bus MUST support a plugin insert chain with per-slot bypass and dry/wet mix. Chain MUST process plugins in insertion order. Bypassed slots MUST pass audio to the next slot.

#### Scenario: Dry/wet blends signal
- GIVEN a compressor at 50% wet
- WHEN audio is processed
- THEN output is 50% dry + 50% compressed

## Metering

The mixer MUST provide per-channel peak and RMS metering (dBFS) updated each process cycle via the audio thread.

#### Scenario: Peak meter reads input level
- GIVEN a channel receiving a sine at −12 dBFS
- WHEN one buffer is processed
- THEN peak metering is approximately −12 dBFS
