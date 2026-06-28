# Tasks: P6 — Plugin Host

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~2,800 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 → PR 2 → PR 3 → PR 4 |
| Delivery strategy | ask-on-risk |
| Chain strategy | stacked-to-main |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: stacked-to-main
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Types, AudioPlugin, ParamManager, TestPlugin | PR 1 | base: feature/p6-plugin-host |
| 2 | CLAP + VST3 hosts, format scanners | PR 2 | base: PR 1 branch |
| 3 | Sandbox, blacklist, scanner, factory | PR 3 | base: PR 2 branch |
| 4 | Adapter, PluginHost wiring, CMake final | PR 4 | base: PR 3 branch; tracker → main |

## Phase 1: Foundation

- [x] 1.1 Create `src/plugin/audio_plugin_types.h` — ParamID, PluginID, PluginCategory, ParameterInfo, ProcessContext
- [x] 1.2 Create `src/plugin/audio_plugin.h` — AudioPlugin abstract interface (lifecycle, process, params, state, latency)
- [x] 1.3 Create `src/plugin/audio_plugin.cc` — ParameterManager with thread-safe get/set, begin/perform/end edit
- [x] 1.4 Update `src/plugin/plugin_host.h` — extend PluginHost stub with factory/scanner/sandbox members
- [x] 1.5 Create `tests/test_common/test_plugin.h/cc` — TestPlugin harness with configurable latency, crash sim
- [x] 1.6 Create `tests/unit/test_plugin_types.cc` — verify ParamID, ParameterInfo, ProcessContext construction

## Phase 2: CLAP + VST3 Hosts

- [x] 2.1 Create `vendor/clap/` — FetchContent CLAP C API v1.2+ headers
- [x] 2.2 Create `src/plugin/clap_host.h/cc` — CLAPHost wrapping clap_plugin_entry, host extensions, process bridge
- [x] 2.3 Create `vendor/vst3sdk/` — VST3 SDK (manual download, ARIA_FEATURE_VST3 guard)
- [x] 2.4 Create `src/plugin/vst3_host.h/cc` — VST3Host with IComponent/IEditController, bus config, IBStream state
- [x] 2.5 Create `src/plugin/format_scanner.h` — FormatScanner abstract interface
- [x] 2.6 Create `src/plugin/clap_scanner.h/cc` — CLAP .clap scanner via clap_plugin_entry factory
- [x] 2.7 Create `src/plugin/vst3_scanner.h/cc` — VST3 .vst3 scanner via IPluginFactory
- [x] 2.8 Add vendor + host sources to CMakeLists.txt (VST3 behind ARIA_FEATURE_VST3)

## Phase 3: Sandbox + Blacklist

- [x] 3.1 Create `src/plugin/plugin_sandbox.h/cc` — per-plugin worker thread, watchdog, terminate + bypass on crash
- [x] 3.2 Create `src/plugin/plugin_blacklist.h/cc` — Warning/Disabled/Banned levels, crash escalation, JSON persist
- [x] 3.3 Add sandbox sources to CMakeLists.txt
- [x] 3.4 Create `tests/unit/test_plugin_sandbox.cc` — watchdog timeout, per-category override, crash kills only plugin
- [x] 3.5 Create `tests/unit/test_plugin_blacklist.cc` — 3 crashes → Disabled, manual ban, corrupt JSON init empty

## Phase 4: Scanner + Factory

- [x] 4.1 Create `src/plugin/plugin_scanner.h/cc` — incremental mtime scan, JSON cache, atomic write, cancel
- [x] 4.2 Create `src/plugin/plugin_scanner_defaults.cc` — platform default scan paths (Windows/macOS/Linux)
- [x] 4.3 Create `src/plugin/plugin_factory.h/cc` — singleton registry, create by ID, category/text search
- [x] 4.4 Create `src/plugin/native_clap_factory.h/cc` — register aria.eq/compressor/reverb/delay/synth as CLAP
- [x] 4.5 Add scanner/factory sources to CMakeLists.txt
- [x] 4.6 Create `tests/unit/test_plugin_scanner.cc` — mtime diff, new/deleted file detection, corrupt cache → full rescan

## Phase 5: Adapter + Wiring

- [x] 5.1 Create `src/plugin/plugin_audio_node.h/cc` — PluginAudioNode adapter (AudioPlugin → AudioNode)
- [x] 5.2 Update `src/plugin/plugin_host.h/cc` — full facade: scan, create, blacklist, sandbox, delegate to subcomponents
- [x] 5.3 Finalize CMakeLists.txt — PLUGIN_SOURCES set, link CLAP/VST3, add aria_core sources
- [x] 5.4 Create `tests/unit/test_plugin_audio_node.cc` — bypass passthrough, param forwarding, adapter lifecycle
- [x] 5.5 Update `tests/CMakeLists.txt` — add aria_plugin_tests executable (Catch2, links aria_core)

## Phase 6: Remaining Tests

- [x] 6.1 Create `tests/unit/test_plugin_params.cc` — ParameterManager thread-safety, begin/end edit undo boundary
- [x] 6.2 Create `tests/unit/test_plugin_factory.cc` — registry search by vendor, category filter, create by ID
- [x] 6.3 Create `tests/unit/test_plugin_host.cc` — PluginHost facade integration with TestPlugin harness
- [x] 6.4 Create `tests/unit/test_native_clap_factory.cc` — built-in EQ/compressor load as AudioPlugin via factory
