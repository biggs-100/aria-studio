# TS Component Library Specification

## Purpose

DAW-specific GPU-rendered widgets built atop the component framework: Fader, MeterBar, ChannelStrip, and TimelineRuler. Each widget SHALL render via the FFI bridge using Skia draw commands.

## Requirements

### Requirement: Fader (Vertical Drag)

The Fader component SHALL render a vertical track with a draggable thumb. Dragging vertically SHALL set a value in [0, 1]. The component SHALL display the current value as a formatted dB string. Touch sensitivity SHALL map finger drag to value with acceleration.

#### Scenario: Vertical drag sets value

- GIVEN a Fader at position x=10, y=10, h=150
- WHEN the user presses at thumb center and drags upward by 30px
- THEN the fader value increases proportionally
- AND the display shows the corresponding dB string (e.g. "-6.0 dB")

#### Scenario: Touch drag with acceleration

- GIVEN a Fader in touch mode
- WHEN the user performs a quick flick gesture
- THEN the value SHALL snap to 0 or 1 based on velocity if at extremes

### Requirement: MeterBar (Peak/RMS)

The MeterBar component SHALL render vertical bars for peak and RMS levels, with color zones (green → yellow → red). Bar height SHALL reflect the signal level. A gradient SHALL transition colors at defined thresholds (-18 dB yellow, -6 dB red).

#### Scenario: Level changes bar height

- GIVEN a MeterBar with peak=-12 dB
- WHEN the signal level updates to peak=-3 dB
- THEN the yellow-red zone fills to the new peak position

#### Scenario: Clip indicator

- GIVEN a MeterBar tracking peak levels
- WHEN peak exceeds 0 dB for any sample
- THEN the top pixel SHALL turn solid red and hold for 2 seconds

### Requirement: ChannelStrip (Composite)

The ChannelStrip component SHALL compose a Fader, MeterBar, mute/solo buttons, pan knob placeholder, and routing label into a fixed-width vertical column. Layout SHALL be defined declaratively within the component's render tree.

#### Scenario: Full channel strip renders all children

- GIVEN a ChannelStrip with default props
- WHEN mounted
- THEN the VNode output contains MeterBar, Fader, mute button, solo button, and routing label nodes

#### Scenario: Narrow strip collapses meter

- GIVEN a ChannelStrip whose parent allocates < 40px width
- WHEN the layout pass completes
- THEN the MeterBar SHALL be hidden and only the Fader remains visible

### Requirement: TimelineRuler (Bar/Beat Ticks)

The TimelineRuler SHALL render bar and beat tick marks with time signature labels and loop markers. Tick density SHALL be driven by zoom level (pixelsPerPPQN). Loop region SHALL render as a highlighted area between start/end markers.

#### Scenario: Bar ticks at zoomed-in view

- GIVEN pixelsPerPPQN = 0.2 (zoomed in)
- WHEN the ruler renders bars 1-4
- THEN each bar boundary has a strong tick with bar number text

#### Scenario: Loop region highlighted

- GIVEN a loop region from bar 2 to bar 4
- WHEN the ruler renders
- THEN bars 2-4 have a semi-transparent overlay
- AND loop start/end markers are drawn at the boundaries
