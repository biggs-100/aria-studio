# Envelope System Specification

## Purpose

Defines envelope clip types — ADSR, AHDSR, DADSR — with gated evaluation, per-segment curve shapes, and configurable timing.

## Requirements

### Requirement: The system SHALL support ADSR envelope shapes

The `EnvelopeClip` MUST support at minimum ADSR (Attack-Decay-Sustain-Release) with configurable timing per segment. Attack, decay, and release times SHALL be in milliseconds. Sustain SHALL be a normalized level (0.0–1.0). Default values SHALL be: attack = 10ms, decay = 300ms, sustain = 0.7, release = 500ms.

#### Scenario: ADSR envelope follows classic shape

- GIVEN an ADSR envelope with attack=100ms, decay=200ms, sustain=0.5, release=400ms
- WHEN the gate is on and evaluated at t=50ms
- THEN the output MUST be rising toward 1.0 during the attack phase
- WHEN evaluated at t=500ms (attack+decay complete)
- THEN the output MUST be at the sustain level of 0.5
- WHEN the gate turns off and evaluated at t=100ms into release
- THEN the output MUST be falling from 0.5 toward 0.0

### Requirement: The system SHALL support AHDSR and DADSR variants

`EnvelopeType` MUST include AHDSR (Attack-Hold-Decay-Sustain-Release) with an additional hold time, and DADSR (Delay-Attack-Decay-Sustain-Release) with an additional pre-delay. The delay SHALL precede attack; hold SHALL follow attack before decay begins.

#### Scenario: AHDSR holds at peak before decay

- GIVEN an AHDSR envelope with attack=50ms, hold=100ms, decay=200ms
- WHEN gate is on and evaluated at t=60ms (attack finished)
- THEN the output MUST remain at 1.0 during the hold phase until t=150ms

#### Scenario: DADSR delays before attack begins

- GIVEN a DADSR envelope with delay=200ms, attack=50ms
- WHEN gate is on and evaluated at t=100ms
- THEN the output MUST be 0.0 (still in delay phase)

### Requirement: Envelopes SHALL use gated evaluation

`evaluate_gated(ppqn, gate_on, gate_start_ppqn)` MUST evaluate the envelope based on the gate signal. When `gate_on` is false and no gate was active, the envelope MUST output 0.0.

#### Scenario: Gate-off before attack completes resets

- GIVEN an ADSR envelope with attack=200ms
- WHEN gate turns on at t=0 and off at t=50ms
- THEN the envelope MUST enter the release phase immediately from the current value

#### Scenario: Sustain holds indefinitely while gate is on

- GIVEN an ADSR envelope with sustain=0.7
- WHEN gate stays on for 10 seconds
- THEN the output MUST remain at 0.7 after attack and decay complete

### Requirement: Each envelope segment SHALL support independent curve shapes

The system MUST support `set_attack_curve()`, `set_decay_curve()`, and `set_release_curve()` using `InterpolationType`. Default SHALL be EaseOut for attack and release, EaseIn for decay.

#### Scenario: EaseOut attack curve rises quickly then slows

- GIVEN an envelope with `attack_curve = EaseOut`, attack=100ms
- WHEN evaluated at t=30ms (30% through attack)
- THEN the value MUST be greater than 0.3 (faster initial rise from EaseOut)

#### Scenario: EaseIn decay curve decays slowly then fast

- GIVEN an envelope with `decay_curve = EaseIn`, decay=200ms
- WHEN evaluated at t=50ms (25% through decay)
- THEN the value MUST be greater than the sustain level + 0.75 × (1.0 - sustain) (slow initial decay)
