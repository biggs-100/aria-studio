# SDD Change: P6 — Plugin Host

## Intent

Load third-party audio plugins (CLAP native, VST3) in the mixer FX chain with crash isolation. Built-in ARIA plugins are also CLAP — no internal fast path.

## Scope

### In Scope
1. Unified `AudioPlugin` abstract interface — lifecycle, audio/MIDI, params, state, latency
2. CLAP host — required extensions (audio, note ports, params, latency, tail, state, MPE)
3. VST3 host — component/controller, bus config, program list
4. Native CLAP plugin factory — EQ, compressor, reverb, delay, synth as CLAP plugins
5. Sandbox Level 1 — per-plugin thread + watchdog; crash kills thread, not DAW
6. Plugin scanning — incremental by file mtime, JSON cache, full rescan
7. Plugin blacklist — auto-track crashes, manual disable/ban
8. Parameter system — thread-safe, automation-ready (begin/end edit, sample offsets)

### Out of Scope
- AU / LV2 hosts
- Sandbox Levels 2-3 (out-of-process)
- Plugin GUI embedding
- Preset manager/browser
- Plugin Delay Compensation

## Capabilities

### New Capabilities
- `plugin-host`: `AudioPlugin` abstraction, CLAP/VST3 hosts, native CLAP factory, parameter system
- `plugin-sandbox`: Level 1 thread isolation, watchdog, crash recovery, plugin blacklist
- `plugin-scanning`: Incremental file discovery by mtime, JSON cache, format scanners (CLAP + VST3)

### Modified Capabilities
None — greenfield project, no existing specs.

## Approach

1. Define `AudioPlugin` base and types in `include/plugin/`
2. Implement `CLAPHost` wrapping CLAP C API with required host extensions
3. Implement `VST3Host` wrapping Steinberg VST3 SDK
4. Build `NativeCLAPFactory` — built-in DSP registers as CLAP
5. Implement `PluginSandbox` — thread pool + watchdog + crash handler
6. Build `PluginScanner` with mtime diff, JSON cache
7. Wire `PluginFactory` into mixer FX rack

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| CLAP extension surface mismatch | Med | Required extensions first; optional behind feature flags |
| Watchdog false positives | Med | Tune timeout per category; user override |

## Rollback Plan

Remove plugin scan paths from config. Mixer falls back to built-in CLAP only. No sandbox threads. Core DAW unaffected.

## Dependencies

- CLAP C API headers (v1.2+, MIT)
- Steinberg VST3 SDK (free for hosts)

## Success Criteria

- [ ] CLAP plugin loads, processes audio, exposes parameters
- [ ] VST3 plugin loads and processes audio
- [ ] Plugin crash in sandbox kills only that plugin (DAW continues)
- [ ] Incremental scan detects new files without full rescan
- [ ] Built-in plugins register and load as CLAP (same path as third-party)
