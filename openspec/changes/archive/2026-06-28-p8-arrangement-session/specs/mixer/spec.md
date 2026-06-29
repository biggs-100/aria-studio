# Delta for Mixer

## MODIFIED Requirements

### Requirement: Mixer Engine

Internal summation MUST use 64-bit float with no intermediate truncation. Supports up to 1024 channels, 128 buses, configurable pan law (equal-power −3 dB default, linear 0 dB, linear −6 dB, stereo balance). Group buses sum assigned channels with parent-child hierarchy: child tracks route into their parent `GroupTrack` bus automatically. VCA tracks control slave fader groups without passing audio; VCA volume contribution combines multiplicatively with each slave's base volume. Group buses MUST support nested hierarchy (group within group).
(Previously: Group buses sum assigned channels, VCA channels control fader groups)

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

Channels MUST route to Master, a Bus, a Track, a Group, or External output. Track→Group routing SHALL be the default for children of a GroupTrack in Summing mode. VCA tracks MUST NOT route audio — they control slave faders only. Sends MUST support pre-fader and post-fader with configurable level. Return channels process send audio to Master.
(Previously: Channels MUST route to Master, a Bus, a Track, or External output)

#### Scenario: Child track routes to parent group by default
- GIVEN an AudioTrack added as a child of GroupTrack "Drums"
- WHEN the mixer queries the track's route target
- THEN output type is `Group` targeting the parent GroupTrack

#### Scenario: VCA track carries no audio
- GIVEN a VCA track assigned to control 3 slave tracks
- WHEN the mixer processes the VCA track's channel
- THEN no audio samples are produced on the VCA output bus
