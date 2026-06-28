# Real-Time Parameter Cache Specification

## Purpose

Defines the lock-free, double-buffered parameter cache that provides sample-accurate parameter reads from the audio thread without mutex contention.

## Requirements

### Requirement: The cache SHALL use lock-free double-buffering

The `ParameterCache` MUST maintain two `Buffer` instances, each an `unordered_map<ParameterID, double>`. An `atomic<uint32_t> active_buffer_` SHALL indicate which buffer is current for reads. `swap_buffers()` SHALL atomically toggle the active index.

#### Scenario: Audio thread reads do not block on main thread writes

- GIVEN a cache with 100 parameters
- WHEN the main thread calls `update_value()` on 50 parameters while the audio thread calls `read_value()` on all 100
- THEN neither call SHALL block, deadlock, or spin

#### Scenario: Double-buffer swap preserves in-flight reads

- GIVEN an audio thread in the middle of `read_value()` for parameter X
- WHEN the main thread calls `swap_buffers()` and updates parameter X in the new write buffer
- THEN the audio thread MUST read the pre-swap value (consistency per callback)

### Requirement: Main thread writes SHALL write to the inactive buffer

`update_value()` MUST always write to the buffer that is NOT `active_buffer_`. After all updates for a processing block, the main thread SHALL call `swap_buffers()`.

#### Scenario: Writes go to inactive buffer, reads from active

- GIVEN `active_buffer_ = 0` and Buffer[0] has vol=0.5, Buffer[1] has vol=0.8
- WHEN `update_value(volume, 0.9)` is called
- THEN volume MUST be written to Buffer[1], and `read_value(volume)` MUST still return 0.5

### Requirement: The cache SHALL be sample-accurate

When the audio thread calls `process_audio_thread()`, the cache MUST evaluate and swap once per buffer, ensuring all parameters within one audio callback see consistent modulation state.

#### Scenario: All parameters in same callback see consistent snapshot

- GIVEN 3 interconnected modulation routes (LFO → Volume, LFO → Pan, Env → LFO Rate)
- WHEN `process_audio_thread(512 samples, sample_pos)` is called
- THEN all 3 targets MUST evaluate using the same snapshot of source values, without cross-callback contamination

### Requirement: The cache SHALL support bulk updates

The system SHOULD provide `update_range()` for batched parameter updates from the modulation engine to minimize atomic operations.

#### Scenario: Bulk update is atomic across swap boundary

- GIVEN `update_range()` called with 20 parameter IDs
- WHEN `swap_buffers()` occurs
- THEN the read side MUST see either all 20 updates or none of them
