# SDD Verify Report

**Change**: p6-plugin-host
**Version**: 1.0
**Mode**: Standard (strict_tdd: false)
**Date**: 2026-06-28

## Completeness

| Metric | Value |
|--------|-------|
| Tasks total | 31 |
| Tasks complete | 30 |
| Tasks incomplete | 1 |
| Source files created | 29 (src/plugin/) + 10 test files |
| Estimated changed lines | ~3,800 |

## Build & Tests Execution

**Build**: ❌ Failed

```text
cmake --build . --target aria_plugin_tests aria_plugin_sandbox_tests aria_plugin_blacklist_tests aria_plugin_scanner_tests --config Debug -- /m

=== COMPILATION ERRORS ===

1) audio_plugin.cc(19): error C2280
   "ParameterManager::Parameter::operator =(const ParameterManager::Parameter &)"
   is deleted because std::atomic<double> has a deleted copy assignment operator.
   Location: audio_plugin.cc:19 — `params_[id] = std::move(param);`
   Fix: Use structured binding or emplace instead of map assignment with atomic member.

2) clap_host.cc(135,139,143): error C2248
   Static function host_get_extension in anonymous namespace cannot access
   private members host_audio_ports_, host_params_, host_mpe_.
   Location: clap_host.cc lines 135, 139, 143
   Fix: Declare host_get_extension as friend, or make extension structs public,
   or move the callback into the class.

3) clap_host.cc(314,322): error C2039
   'extension_data' is not a member of 'clap_plugin_descriptor'
   The vendored CLAP v1.2 headers (vendor/clap/include/clap/clap.h:140) don't
   have an extension_data field.
   Location: clap_host.cc lines 314, 322
   Fix: Remove references to extension_data — CLAP v1.2 doesn't expose extensions
   through the descriptor.

4) clap_host.cc(420): error C2440
   Cannot convert 'float *const *' to 'float **' (const-correctness)
   clap_audio_buffer.data32 expects float** but process() receives const float* const*.
   Location: clap_host.cc:420
   Fix: const_cast or change signature.
```

**Tests**: ❌ Could not run (build failed)

**Missing test target**: `test_plugin_types.cc` (14 tests) has NO CMake test executable target. It exists in the source tree but is never compiled or run.

**Coverage**: ➖ Not available (build failed)

## Spec Compliance Matrix

### Plugin Host Spec — 8 Requirements, 10 Scenarios

| Requirement | Scenario | Test | Result |
|-------------|----------|------|--------|
| REQ-01: AudioPlugin Lifecycle | Standard lifecycle completes | `test_native_clap_factory.cc` > "Built-in EQ lifecycle completes" | ✅ COMPLIANT |
| REQ-01: AudioPlugin Lifecycle | Unsupported buffer size fails | (no covering test) | ❌ UNTESTED |
| REQ-02: Audio Processing & MIDI | Bypassed plugin passes audio through | `test_plugin_audio_node.cc` > passthrough test | ⚠️ PARTIAL — no `set_bypass` test, AudioPlugin lacks bypass API |
| REQ-02: Audio Processing & MIDI | Sample-offset parameter changes apply | (no covering test) | ❌ UNTESTED — ProcessContext has no parameter_changes |
| REQ-03: Parameter System | Automation edit creates undo boundary | `test_plugin_params.cc` > "begin/perform/end edit" | ✅ COMPLIANT |
| REQ-03: Parameter System | Concurrent reads do not block | `test_plugin_params.cc` > "Concurrent reads" | ✅ COMPLIANT |
| REQ-04: State Serialization | State round-trip restores exactly | `test_native_clap_factory.cc` > "state round-trip" | ⚠️ PARTIAL — tests param values only, not bypass state (bypass not implemented) |
| REQ-05: Native CLAP Plugin Factory | Built-in loads same as third-party | `test_native_clap_factory.cc` > multi | ✅ COMPLIANT |
| REQ-06: CLAP Host Extensions | Plugin queries latency on activation | (no covering test) | ❌ UNTESTED — also CLAPHost extension caching is incomplete |
| REQ-07: VST3 Component/Controller | Multi-bus VST3 enumerates correctly | (no covering test) | ❌ UNTESTED — VST3 behind feature flag |
| REQ-08: PluginFactory Registry | Search finds plugin by vendor | `test_plugin_factory.cc` > "Search finds by vendor" | ✅ COMPLIANT |

**Compliance summary**: 5/11 ✅, 1 ⚠️ PARTIAL, 5 ❌ UNTESTED

### Plugin Sandbox Spec — 6 Requirements, 8 Scenarios

| Requirement | Scenario | Test | Result |
|-------------|----------|------|--------|
| REQ-01: Thread Isolation | Plugin hang does not stall others | `test_plugin_sandbox.cc` > "Crash kills only" | ⚠️ PARTIAL — tests isolation but not multi-plugin stall scenario |
| REQ-01: Thread Isolation | Plugin crash kills only its thread | `test_plugin_sandbox.cc` > "Crash kills only" | ✅ COMPLIANT |
| REQ-02: Watchdog Timeout | Watchdog terminates hung plugin | `test_plugin_sandbox.cc` > "Watchdog terminates hung" | ✅ COMPLIANT |
| REQ-02: Watchdog Timeout | Per-category timeout override works | `test_plugin_sandbox.cc` > "Per-category timeout" | ✅ COMPLIANT |
| REQ-03: Crash Recovery | Crashed plugin restarts on next cycle | (no covering test) | ❌ UNTESTED |
| REQ-04: Blacklist Levels | Three crashes escalate to Disabled | `test_plugin_blacklist.cc` > "Three crashes escalate" | ✅ COMPLIANT |
| REQ-04: Blacklist Levels | Manual ban overrides auto escalation | `test_plugin_blacklist.cc` > "Manual ban overrides" | ✅ COMPLIANT |
| REQ-05: Blacklist Persistence | Corrupt file initializes empty | `test_plugin_blacklist.cc` > "Corrupt JSON" | ✅ COMPLIANT |
| REQ-06: Manual Management | Cleared entry allows reload | `test_plugin_blacklist.cc` > "Clear entry allows reload" | ✅ COMPLIANT |

**Compliance summary**: 7/9 ✅, 1 ⚠️ PARTIAL, 1 ❌ UNTESTED

### Plugin Scanning Spec — 5 Requirements, 8 Scenarios

| Requirement | Scenario | Test | Result |
|-------------|----------|------|--------|
| REQ-01: Incremental Scan by mtime | New plugin discovered without rescan | `test_plugin_scanner.cc` > "Incremental discovers new" | ✅ COMPLIANT |
| REQ-01: Incremental Scan by mtime | Deleted plugin removed from cache | `test_plugin_scanner.cc` > "Deleted plugin removed" | ✅ COMPLIANT |
| REQ-01: Incremental Scan by mtime | Updated plugin re-scanned | `test_plugin_scanner.cc` > "Updated plugin re-scanned" | ✅ COMPLIANT |
| REQ-02: JSON Cache | Corrupt cache triggers full rescan | `test_plugin_scanner.cc` > "Missing cache" | ⚠️ PARTIAL — tests missing cache but not corrupt cache → full rescan |
| REQ-02: JSON Cache | Atomic write prevents corruption | (no covering test) | ❌ UNTESTED |
| REQ-03: Format Scanners | CLAP scanner enumerates all factory plugins | (no covering test — needs real .clap) | ❌ UNTESTED |
| REQ-03: Format Scanners | VST3 single effect scans correctly | (no covering test — VST3 behind flag) | ❌ UNTESTED |
| REQ-04: Default Scan Paths | Custom path adds plugins | (no covering test) | ❌ UNTESTED — also scanner lacks `add_scan_path()` |
| REQ-05: Scan Cancellation | Partial scan preserves prior results | `test_plugin_scanner.cc` > "Cancel scan" | ⚠️ PARTIAL — tests cancel but doesn't verify partial results preserved |

**Compliance summary**: 3/9 ✅, 2 ⚠️ PARTIAL, 4 ❌ UNTESTED

### Overall Spec Compliance

| Metric | Value |
|--------|-------|
| Total scenarios | 26 |
| ✅ COMPLIANT | 15 (57.7%) |
| ⚠️ PARTIAL | 4 (15.4%) |
| ❌ UNTESTED | 7 (26.9%) |
| ❌ FAILING | 0 |

## Correctness (Static Evidence)

| Requirement | Status | Notes |
|------------|--------|-------|
| AudioPlugin abstract interface | ✅ Implemented | Lifecycle, params, state, latency all defined. Missing: `set_midi_events()`, `set_bypass()/is_bypassed()`, `tail_samples()` from spec. |
| ParameterManager thread-safe | ✅ Implemented | shared_mutex + atomic<double>, begin/perform/end edit with editing guard |
| ProcessContext | ✅ Implemented | Simplified from spec: no parameter_changes, tempo, transport flags |
| CLAPHost loading and lifecycle | ✅ Implemented | Full CLAP init/activate/deactivate/process. Extension caching is incomplete — all extension pointers end up null |
| CLAPHost extension handling | ❌ Buggy | `host_get_extension` accesses private members (compile error). Extension caching uses `nullptr` patterns. `plugin_audio_ports_` assignment at line 314 is always null |
| VST3Host | ✅ Implemented | Behind ARIA_FEATURE_VST3 guard. Component/controller separation, bus config, IBStream state |
| PluginSandbox | ✅ Implemented | Worker thread, watchdog timeout, crash → bypass cycle. After crash, detaches thread and creates new one |
| PluginBlacklist | ✅ Implemented | Warning→Disabled→Banned escalation, manual ban, JSON persistence with atomic write |
| PluginScanner | ✅ Implemented | Incremental mtime scan, multi-extension support, JSON cache with atomic write, cancel support |
| PluginFactory | ✅ Implemented | Singleton registry, search by name/vendor/ID with case-insensitive matching, category filter |
| Native CLAP factory | ✅ Implemented | 5 built-in plugins: EQ (8 params), Compressor (6), Reverb (5), Delay (4), Synth (8). All DSP is pass-through |
| PluginAudioNode adapter | ✅ Implemented | AudioBuffer → channel pointer conversion, sandbox integration, bypass passthrough |
| PluginHost facade | ✅ Implemented | Wires factory, scanner, sandbox, blacklist. Create/destroy/process instances |

## Coherence (Design)

| Decision | Followed? | Notes |
|----------|-----------|-------|
| **Thread model**: Per-plugin worker + watchdog | ✅ Yes | PluginSandbox uses dedicated thread, async process + timeout |
| **CLAP wrapping**: Thin struct wrappers | ✅ Yes | CLAPHost directly uses clap C API structs, clap_host_bridge pattern |
| **AudioPlugin ↔ AudioNode**: Adapter pattern | ✅ Yes | PluginAudioNode wraps AudioPlugin, converts buffer formats |
| **Watchdog timeout**: Configurable per category | ⚠️ Partial | Sandbox has set_timeout() but PluginHost doesn't implement per-category defaults |
| **State persistence**: Plugin-owned byte vector | ✅ Yes | save_state/load_state use std::vector<uint8_t> |
| **Plugin factory**: Registry singleton | ✅ Yes | PluginFactory::instance() singleton, format scanners register creators |
| Interfaces match design | ⚠️ Partial | AudioPlugin interface simplified vs design (no set_bypass/is_bypassed on base, no tail_samples, process uses ProcessContext instead of separate args for channels) |

## Issues Found

### CRITICAL

1. **Build failure — 4 compilation errors** prevent any test from running:
   - `audio_plugin.cc:19` — `std::atomic<double>` deleted copy-assignment in map insertion → `params_[id] = std::move(param)` fails
   - `clap_host.cc:135,139,143` — anonymous namespace function accesses private members of CLAPHost
   - `clap_host.cc:314,322` — `clap_plugin_descriptor.extension_data` doesn't exist in CLAP v1.2
   - `clap_host.cc:420` — const-correctness: `const float* const*` → `float**`

2. **Missing CMake test target**: `test_plugin_types.cc` (14 tests for PR 1 foundation types) is never wired into any test executable — no `aria_plugin_types_tests` target exists. Task 1.6 test file exists but is not buildable.

3. **CLAPHost extension caching is broken**: The `plugin_audio_ports_` assignment at line 314 produces null always (`desc->extension_data` doesn't exist). All cached extension pointers (`plugin_params_`, `plugin_latency_`, `plugin_tail_`, `plugin_state_`) remain null. This means parameter queries, state save/load, and latency reporting all silently fail at runtime on real CLAP plugins.

4. **7 spec scenarios have zero test coverage** (UNTESTED):
   - Unsupported buffer size rejection
   - Sample-offset parameter changes
   - CLAP latency extension query
   - Multi-bus VST3 enumeration
   - Crashed plugin restart on cycle
   - Atomic cache write corruption protection
   - CLAP scanner enumerating multi-plugin factory
   - VST3 scanner single effect detection

### WARNING

1. **Spec vs implementation drift**: Significant gaps exist between spec requirements and code:
   - `AudioPlugin` missing `set_midi_events()` (spec: "MUST accept via set_midi_events()")
   - `AudioPlugin` missing `set_bypass()`/`is_bypassed()` (spec: bypass scenario)
   - `AudioPlugin` missing `tail_samples()` (spec requirement)
   - `ProcessContext` missing `parameter_changes`, `tempo`, `is_playing`, `is_recording` (per design)
   - `PluginScanner` missing `add_scan_path()`/`remove_scan_path()` (spec: "Users MUST add and remove custom paths")

2. **No coverage reporting**: No coverage tool configured or available.

3. **DSP placeholders**: All 5 native plugins (EQ, compressor, reverb, delay, synth) use pass-through audio processing — actual DSP algorithm implementation is deferred.

4. **VST3 untestable**: Both VST3Host and VST3Scanner are behind `ARIA_FEATURE_VST3` guard which is OFF, meaning VST3 scenarios are structurally untestable.

### SUGGESTION

1. **Add `aria_plugin_types_tests` target** to `tests/CMakeLists.txt` for `test_plugin_types.cc`.

2. **Consider adding `set_bypass` and `is_bypassed`** to `AudioPlugin` base class to match the spec design that explicitly lists them.

3. **Add `tail_samples()`** to `AudioPlugin` base to match the spec's CLAP tail extension requirement.

4. **The sandbox crash recovery test** ("Crashed plugin restarts on next cycle") is unimplemented — `handle_crash()` already creates a new worker thread, so a test should verify that `process_async()` works again after a crash.

5. **CLAPHost extension query needs rewriting** to use the standard CLAP v1.2 pattern: extensions are queried via `clap_plugin_extensions` or by checking the plugin descriptor features. The current code tries non-existent `extension_data`.

6. **File naming inconsistency**: Files use `plugin_audio_node.h/cc` but the class is `PluginAudioNode`. Consider renaming to `plugin_audio_node.h` → `plugin_audio_node.h` (already correct) for consistency with `audio_plugin.h`.

7. **Static-cast index to ParamID**: `PluginAudioNode::set_parameter()` uses `static_cast<plugin::ParamID>(index)` which assumes sequential mapping from 0 — fragile if ParameterManager uses non-sequential IDs.

## Verdict

**FAIL**

The build does not compile (4 errors in 2 files), 14 foundational tests are excluded from the build system, CLAPHost extension caching is broken at runtime for real plugins, 7 spec scenarios have zero coverage, and API gaps exist between the unified `AudioPlugin` interface and the spec/design requirements. The code structure and test quantity are commendable (well-separated components, good test coverage for what is wired), but the issues are structural — not cosmetic — and prevent verification from completing.
