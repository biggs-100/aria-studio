# Modulation Matrix Specification

## Purpose

Defines the modulation routing system — source-to-target connections, per-connection amount and polarity, nested modulation chains up to 8 levels deep.

## Requirements

### Requirement: The matrix SHALL connect modulation sources to parameter targets

The `ModulationMatrix` MUST support connecting any `ModulationSource*` to any `ParameterID`. Each connection SHALL include amount (0.0–1.0), offset, min/max clamp, polarity (Unipolar/Bipolar), and bypass state. The matrix MUST support querying connections by target or by source.

#### Scenario: Connect LFO to filter cutoff with amount 0.5

- GIVEN an LFO source (bipolar, output -1.0 to +1.0) and a filter cutoff with base value 0.5
- WHEN the connection amount is set to 0.5 and the LFO evaluates to 1.0
- THEN the modulated output MUST be 0.5 + (1.0 × 0.5 × 0.5) = 0.75

#### Scenario: Bypassed connection has no effect

- GIVEN an LFO connected to volume with amount 1.0
- WHEN `set_bypass(entry_id, true)` is called and LFO evaluates to 0.0
- THEN the volume MUST stay at its base value

### Requirement: Polarity SHALL be configurable per connection

A `Unipolar` connection maps source output 0.0–1.0. A `Bipolar` connection maps source output -1.0–1.0. The system MUST clamp the final value to 0.0–1.0.

#### Scenario: Unipolar modulation only adds upward

- GIVEN a unipolar modulation with amount 1.0 on a parameter at base 0.5
- WHEN the source evaluates to 0.5
- THEN the result MUST be 0.5 + (0.5 × 1.0) = 1.0

#### Scenario: Bipolar modulation can push both directions

- GIVEN a bipolar modulation with amount 0.4 on a parameter at base 0.5
- WHEN the source evaluates to -0.5
- THEN the result MUST be 0.5 + (-0.5 × 0.4) = 0.3

### Requirement: Nested modulation SHALL support 8 levels of depth

A `NestedModulation` source MUST modulate parameters of other modulation sources (e.g., LFO rate). The system MUST enforce a max depth of 8 and MUST detect and reject circular chains.

#### Scenario: Envelope follower modulates LFO rate

- GIVEN a chain: Envelope Follower → LFO 1 Rate → Filter Cutoff
- WHEN audio amplitude increases from 0.2 to 0.8
- THEN LFO 1's rate MUST increase proportionally

#### Scenario: Circular chain is rejected

- GIVEN Source A modulates Source B, and Source B modulates Source A
- WHEN `add_link()` is called creating the cycle
- THEN the system MUST reject the connection and return an error

### Requirement: The stack SHALL evaluate modulations in insertion order

`ModulationStack` MUST apply connected sources sequentially: each subsequent modulation operates on the result of the previous. The stack MUST support bypass per entry.

#### Scenario: Stack applies modulations in order

- GIVEN a stack with LFO (amount 0.5, bipolar) and EnvelopeFollower (amount 1.0, unipolar) on filter cutoff (base 0.5)
- WHEN LFO evaluates to 1.0 and envelope follower to 0.3
- THEN the result MUST be ((0.5 + (1.0 × 0.5)) × 0.3 × 1.0) = 0.3

#### Scenario: Stack with all entries bypassed returns base value

- GIVEN a stack with 3 modulations, all bypassed
- WHEN evaluated with any source values
- THEN the output MUST equal the base value
