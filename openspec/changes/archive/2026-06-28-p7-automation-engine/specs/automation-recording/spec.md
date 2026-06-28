# Automation Recording Specification

## Purpose

Defines real-time automation capture: ring buffer recording from the audio thread, configurable smoothing, Douglas-Peucker point reduction, and post-recording clip editing.

## Requirements

### Requirement: The recorder SHALL capture parameter values from the audio thread

`AutomationRecorder` MUST provide a ring buffer (default 44100 samples) that stores `{sample_position, value}` pairs. `record_value()` MUST be callable from the audio thread. `start_recording()` and `stop_recording()` MUST be called from the main thread.

#### Scenario: Real-time capture stores values in ring buffer

- GIVEN a recorder started on an armed lane
- WHEN 1000 samples are produced with varying values
- THEN `recorded_samples()` MUST return 1000 AND the buffer MUST contain all sample positions

#### Scenario: Ring buffer wraps without crashing

- GIVEN a recorder that has been recording for 2 seconds at 44.1kHz
- WHEN the write index exceeds the ring buffer size
- THEN older samples MUST be overwritten without memory corruption

### Requirement: Smoothing SHALL be applied to incoming data

The recorder MUST provide `set_smoothing(ms)` for low-pass filtering of incoming recorded values. Default smoothing SHALL be 10ms.

#### Scenario: Smoothing reduces jitter in recorded values

- GIVEN a knob being turned with ±2% sample-to-sample jitter
- WHEN smoothing is set to 10ms
- THEN the recorded output MUST have fewer abrupt value changes than the raw input

### Requirement: Douglas-Peucker reduction SHALL simplify dense recordings

The recorder MUST provide `reduce_points()` using the Ramer-Douglas-Peucker algorithm with configurable tolerance (default 0.01). The system SHALL support three modes: `None` (keep all), `ReduceOnStop` (simplify after recording), and `Adaptive` (simplify during recording).

#### Scenario: ReduceOnStop reduces 1800 points to ~20-50

- GIVEN a 5-second recording of a smooth knob turn producing ~1800 raw points
- WHEN `ReduceOnStop` mode is used with tolerance 0.01
- THEN the reduced clip MUST contain between 20 and 50 points while preserving the curve shape

#### Scenario: Adaptive reduction maintains real-time performance

- GIVEN a recorder in `Adaptive` mode
- WHEN recording continuously for 10 seconds
- THEN the audio thread callback MUST never block for reduction work

#### Scenario: No reduction preserves every sample

- GIVEN a recorder in `None` reduction mode
- WHEN 5000 samples are recorded
- THEN `recorded_clip()->point_count()` MUST equal 5000

### Requirement: Post-recording editing SHALL be supported

After recording stops, the `AutomationClip` SHALL support manual point editing: add, remove, move, quantize, offset, and scale operations. The recorder MUST return the clip via `recorded_clip()`.

#### Scenario: Move a recorded point

- GIVEN a recorded clip with a point at PPQN 480, value 0.5
- WHEN `move_point(480, 960, 0.7)` is called
- THEN the point MUST be at PPQN 960 with value 0.7
