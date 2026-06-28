# ARIA DAW — Scripting System Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: Lua 5.4
> **Binding**: C-API via lua.h

---

## Table of Contents

1. [Overview](#1-overview)
2. [Lua Runtime](#2-lua-runtime)
3. [Public API](#3-public-api)
4. [Script Types](#4-script-types)
5. [MIDI Scripts](#5-midi-scripts)
6. [Audio Scripts](#6-audio-scripts)
7. [UI Scripts](#7-ui-scripts)
8. [Automation Scripts](#8-automation-scripts)
9. [Macro System](#9-macro-system)
10. [Script Manager](#10-script-manager)
11. [Security & Sandboxing](#11-security--sandboxing)
12. [API Reference](#12-api-reference)
13. [RFC Index](#13-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Scripting System allows users to extend ARIA DAW using Lua scripts. It provides a comprehensive public API for MIDI processing, audio generation, UI creation, automation, and macro recording. Lua is a first-class citizen — not an afterthought.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Script types | MIDI, Audio, UI, Automation, Macro |
| API coverage | Full access to project model, transport, MIDI, audio |
| Performance | < 1ms per script per audio buffer |
| Security | Sandboxed environment, resource limits |
| Hot-reload | Modify script without restart |
| Distribution | Single .lua file, no dependencies |

### 1.3. Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     Lua Runtime                                │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │                  Lua VM (lua_State)                      │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐  │  │
│  │  │ Sandbox  │  │ Library  │  │  Script Instances    │  │  │
│  │  │ (limits) │  │ (API)    │  │  script1.lua         │  │  │
│  │  └──────────┘  └──────────┘  │  script2.lua         │  │  │
│  │                              │  script3.lua         │  │  │
│  │                              └──────────────────────┘  │  │
│  └─────────────────────────────────────────────────────────┘  │
│                           │                                    │
│  ┌────────────────────────▼─────────────────────────────────┐ │
│  │              C++ → Lua Bridge (lua_CFunction)             │ │
│  │  aria.get_track_count()  aria.create_note()              │ │
│  │  aria.get_transport()    aria.add_automation_point()     │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Lua Runtime

### 2.1. VM Management

```cpp
class LuaRuntime {
public:
    LuaRuntime();
    ~LuaRuntime();

    // Initialize the Lua VM
    bool init(const LuaConfig& config);
    void shutdown();
    
    // Create a new script instance
    ScriptInstance* create_script(const std::string& source_path);
    void destroy_script(ScriptInstance* script);
    
    // Load script from string
    ScriptInstance* load_string(const std::string& source,
                                 const std::string& name);
    
    // Reload a script (hot-reload)
    bool reload_script(ScriptInstance* script);
    
    // Garbage collection
    void gc_full();
    void gc_step();
    uint64_t gc_memory_usage() const;
    
    // Performance
    struct ScriptStats {
        uint64_t memory_bytes;
        float avg_execution_ms;
        uint32_t instruction_count;
        uint32_t error_count;
    };
    ScriptStats stats(ScriptInstance* script) const;

private:
    lua_State* L_;
    LuaConfig config_;
    std::vector<std::unique_ptr<ScriptInstance>> scripts_;
    
    // Pre-loaded libraries
    void register_aria_api();
    void register_safe_libraries();
    void setup_error_handlers();
};

struct LuaConfig {
    uint32_t max_memory_bytes = 50 * 1024 * 1024;  // 50MB per VM
    uint32_t max_instructions_per_frame = 10000;
    uint32_t max_script_count = 128;
    float timeout_seconds = 5.0;
    bool enable_io = false;          // File I/O (dangerous)
    bool enable_os = false;          // OS commands (dangerous)
    bool enable_debug = true;        // Debug library
    bool enable_jit = true;          // LuaJIT if available
};
```

### 2.2. Script Instance

```cpp
class ScriptInstance {
public:
    ScriptInstance(const std::string& source, lua_State* L);
    
    // Script info
    std::string name() const;
    std::string description() const;
    std::string author() const;
    std::string version() const;
    ScriptType type() const;
    
    // Lifecycle hooks
    bool init();
    void destroy();
    
    // Processing hooks (called by host)
    void on_midi_event(const MidiEvent& event);
    void on_audio_block(AudioBlock& block);
    void on_note(NoteEvent& event);
    void on_parameter_change(ParamID id, double value);
    void on_transport_change(TransportState state);
    void on_timer(double dt_seconds);
    
    // UI hooks
    void on_draw(SkiaCanvas* canvas, const Rect& bounds);
    void on_mouse_event(const MouseEvent& event);
    void on_key_event(const KeyEvent& event);
    
    // Error handling
    bool has_error() const;
    std::string last_error() const;
    int error_count() const;
    
    // State
    void save_state(const std::string& key, const std::string& value);
    std::string load_state(const std::string& key);
    
    // Hot reload
    bool reload(const std::string& new_source);

private:
    lua_State* L_;
    std::string source_;
    std::string file_path_;
    
    // Hook references (Lua registry)
    int init_ref_;
    int process_ref_;
    int draw_ref_;
    int event_refs_[EventType::Count];
    
    // Error state
    std::string last_error_;
    int error_count_ = 0;
    bool has_error_ = false;
    
    // User state storage
    std::unordered_map<std::string, std::string> user_state_;
};
```

---

## 3. Public API

### 3.1. API Categories

```lua
-- ARIA Lua API modules:

-- Project access
aria.project.get_track_count()
aria.project.get_track_name(track_id)
aria.project.add_track(name, type)
aria.project.delete_track(track_id)

-- Transport
aria.transport.play()
aria.transport.stop()
aria.transport.record()
aria.transport.get_position()       -- returns ppqn
aria.transport.set_position(ppqn)
aria.transport.get_tempo()
aria.transport.set_tempo(bpm)

-- MIDI
aria.midi.send_note(channel, note, velocity, duration)
aria.midi.send_cc(channel, cc, value)
aria.midi.get_input_count()
aria.midi.get_output_count()
aria.midi.route(from_port, to_port)

-- Audio
aria.audio.get_track_audio(track_id, buffer)
aria.audio.get_peak_level(track_id)
aria.audio.get_rms_level(track_id)
aria.audio.set_track_volume(track_id, db)
aria.audio.set_track_pan(track_id, pan)

-- Clip/Note editing
aria.clip.create_midi_clip(track_id, length_ppqn)
aria.clip.add_note(clip_id, pitch, start_ppqn, duration_ppqn, velocity)
aria.clip.remove_note(clip_id, note_index)
aria.clip.get_notes(clip_id)
aria.clip.quantize(clip_id, grid)
aria.clip.transpose(clip_id, semitones)

-- Automation
aria.automation.create_lane(track_id, parameter)
aria.automation.add_point(lane_id, ppqn, value)
aria.automation.clear_lane(lane_id)

-- UI
aria.ui.show_message(text)
aria.ui.show_confirm(text)           -- returns boolean
aria.ui.create_window(title, width, height)
aria.ui.add_button(parent, label, callback)
aria.ui.add_slider(parent, min, max, callback)
aria.ui.add_label(parent, text)
aria.ui.canvas_get_size()            -- returns width, height
aria.ui.canvas_draw_rect(x, y, w, h, color)
aria.ui.canvas_draw_text(x, y, text, size, color)
aria.ui.canvas_draw_line(x1, y1, x2, y2, color, width)

-- Script
aria.script.get_name()
aria.script.set_name(name)
aria.script.get_path()
aria.script.log(message)
aria.script.set_timer(interval_ms)
aria.script.sleep(ms)                -- in async context

-- System
aria.system.get_version()
aria.system.get_platform()           -- "windows", "macos", "linux"
aria.system.get_user_data_path()
aria.system.execute_command(cmd)     -- sandboxed, limited

-- Preferences
aria.prefs.get(key, default)
aria.prefs.set(key, value)
```

### 3.2. Example: MIDI Arpeggiator

```lua
-- Simple MIDI arpeggiator script
-- Name: Basic Arpeggiator
-- Author: ARIA User
-- Type: midi_effect

local notes = {}
local pattern = {1, 2, 3, 4}  -- Up pattern
local step = 1
local rate = 0.25  -- Quarter note

function init()
    -- Called when script is loaded
    aria.script.log("Arpeggiator loaded")
    aria.script.set_timer(10)  -- Check every 10ms
end

function on_midi_event(event)
    -- Store held notes
    if event.type == "note_on" then
        notes[event.note] = event.velocity
    elseif event.type == "note_off" then
        notes[event.note] = nil
    end
end

function on_timer(dt)
    -- Generate arpeggio from held notes
    if #notes == 0 then return end
    
    local sorted = {}
    for note, vel in pairs(notes) do
        table.insert(sorted, note)
    end
    table.sort(sorted)
    
    local note_index = pattern[step]
    if note_index > #sorted then
        note_index = ((note_index - 1) % #sorted) + 1
    end
    
    local pitch = sorted[note_index]
    local transport = aria.transport.get_position()
    
    -- Play note
    aria.midi.send_note(1, pitch, notes[pitch], rate * 0.9)
    
    step = step + 1
    if step > #pattern then step = 1 end
end

function on_parameter_change(id, value)
    if id == "rate" then
        rate = value
    elseif id == "pattern" then
        -- Parse pattern string
    end
end
```

### 3.3. Example: Note Colorizer

```lua
-- Color notes by velocity
-- Name: Velocity Colorizer
-- Type: midi_script

function on_note(note)
    -- Color notes based on velocity
    if note.velocity > 100 then
        note.color = "#FF4A4A"  -- Red (hot)
    elseif note.velocity > 70 then
        note.color = "#FFAA00"  -- Orange
    elseif note.velocity > 40 then
        note.color = "#7AFF7A"  -- Green
    else
        note.color = "#4A9EFF"  -- Blue (cold)
    end
    return note  -- Return modified note
end
```

### 3.4. Example: Custom UI Panel

```lua
-- Custom chord generator panel
-- Name: Chord Helper
-- Type: ui_script

function init()
    local w = aria.ui.create_window("Chord Helper", 300, 200)
    
    aria.ui.add_label(w, "Root Note:", 10, 10)
    local root = aria.ui.add_dropdown(w, {"C", "D", "E", "F", "G", "A", "B"}, 100, 10)
    
    aria.ui.add_label(w, "Type:", 10, 40)
    local ctype = aria.ui.add_dropdown(w, {"Major", "Minor", "Dom7", "Maj7", "Min7"}, 100, 40)
    
    aria.ui.add_button(w, "Insert Chord", 10, 80, function()
        insert_chord(root:get(), ctype:get())
    end)
end

function insert_chord(root, chord_type)
    -- Generate chord notes
    local intervals = {
        Major = {0, 4, 7},
        Minor = {0, 3, 7},
        Dom7 = {0, 4, 7, 10},
        Maj7 = {0, 4, 7, 11},
        Min7 = {0, 3, 7, 10}
    }
    
    local root_note = {C=0, D=2, E=4, F=5, G=7, A=9, B=11}[root]
    local pos = aria.transport.get_position()
    local clip = aria.clip.get_active_clip()
    
    for i, interval in ipairs(intervals[chord_type]) do
        local pitch = root_note + interval + 60  -- Middle octave
        aria.clip.add_note(clip, pitch, pos + (i-1) * 240, 240, 100)
    end
end
```

---

## 4. Script Types

### 4.1. Script Categories

| Type | Processing | UI | Use Case |
|---|---|---|---|
| `midi_effect` | MIDI in → MIDI out | Optional | Arpeggiators, chord generators, transforms |
| `midi_generator` | MIDI out only | Optional | Step sequencers, pattern generators |
| `audio_effect` | Audio in → Audio out | Optional | Custom DSP, weird effects |
| `audio_generator` | Audio out only | Required | Synths, drum machines |
| `ui_script` | Event-driven | Required | Custom panels, tools |
| `automation` | Value in → Value out | Optional | Envelope generators, LFOs |
| `macro` | Recording/playback | None | Macro recording |

### 4.2. Script Manifest

```lua
-- Every script has a metadata block at the top:

-- Name: Script Name
-- Author: Author Name
-- Version: 1.0
-- Type: midi_effect
-- Description: Description of what the script does
-- Tags: arpeggiator, chord, utility
-- Icon: music_note  -- Material icon name
-- Parameters:
--   rate: { type: float, min: 0.0, max: 1.0, default: 0.5 }
--   pattern: { type: enum, values: ["up","down","random"], default: "up" }
```

---

## 5. MIDI Scripts

### 5.1. MIDI Processing

```cpp
class MidiScriptProcessor {
public:
    // Process MIDI through a chain of MIDI scripts
    void process_midi(const MidiEventList& input,
                      MidiEventList& output,
                      const std::vector<ScriptInstance*>& scripts);
    
    // Each script in the chain:
    // 1. Receives MIDI events via on_midi_event()
    // 2. Can generate new events via aria.midi.send_*()
    // 3. Can suppress events by not forwarding them
    // 4. Output of script N → input of script N+1
    
    // Processing order is deterministic:
    // Scripts are sorted by insertion order in the chain
};
```

### 5.2. MIDI Script Example: Transposer

```lua
-- Name: Simple Transposer
-- Type: midi_effect
-- Parameters:
--   semitones: { type: int, min: -24, max: 24, default: 0 }
--   scale_aware: { type: bool, default: false }

local semitones = 0
local scale_aware = false

function on_parameter_change(id, value)
    if id == "semitones" then semitones = value end
    if id == "scale_aware" then scale_aware = value end
end

function on_midi_event(event)
    if event.type == "note_on" or event.type == "note_off" then
        local new_note = event.note + semitones
        
        if scale_aware then
            -- Snap to C major scale
            local scale = {0, 2, 4, 5, 7, 9, 11}
            local octave = math.floor(new_note / 12)
            local degree = new_note % 12
            local nearest = 0
            local min_dist = 12
            
            for _, s in ipairs(scale) do
                local dist = math.abs(degree - s)
                if dist < min_dist then
                    min_dist = dist
                    nearest = s
                end
            end
            
            new_note = octave * 12 + nearest
        end
        
        aria.midi.send_note(event.channel, new_note, event.velocity, event.duration)
    else
        -- Forward non-note events
        aria.midi.send_event(event)
    end
end
```

---

## 6. Audio Scripts

### 6.1. Audio Processing

```cpp
class AudioScriptProcessor {
public:
    // Process audio through a script
    // Script receives audio block via on_audio_block()
    // Script can modify the block in-place
    // Script can generate new audio from scratch
    // Script runs on a non-real-time thread (safe for GC)
    
    void process_audio(AudioBlock& block,
                       ScriptInstance* script);
    
    // Performance constraints:
    // - Script must return within 1ms
    // - Audio blocks are 32-1024 samples
    // - If script exceeds time budget → bypass until next block
};
```

### 6.2. Audio Script Example: Bit Crusher

```lua
-- Name: Lua Bit Crusher
-- Type: audio_effect
-- Parameters:
--   bits: { type: int, min: 1, max: 24, default: 8 }
--   rate: { type: int, min: 1, max: 32, default: 4 }

local bits = 8
local rate = 4
local counter = 0
local last_sample = 0

function on_audio_block(block)
    local q_levels = 2 ^ (bits - 1)
    local sample_rate = block.sample_rate
    local step = math.floor(sample_rate / (rate * 100))
    
    for ch = 0, block.channels - 1 do
        local samples = block:get_channel(ch)
        for i = 0, block.frames - 1 do
            counter = counter + 1
            if counter >= step then
                counter = 0
                -- Quantize
                samples[i] = math.floor(samples[i] * q_levels + 0.5) / q_levels
                last_sample = samples[i]
            else
                -- Sample and hold
                samples[i] = last_sample
            end
        end
    end
    
    return block
end
```

---

## 7. UI Scripts

### 7.1. Custom UI

```cpp
class UIScript {
public:
    // Scripts can create custom UI panels:
    // 1. Create a window with aria.ui.create_window()
    // 2. Add widgets: button, slider, dropdown, label, canvas
    // 3. Handle events with callbacks
    // 4. Draw custom content in on_draw()
    
    // Widgets are not real OS widgets — they are GPU-rendered
    // shapes rendered by the script's on_draw() callback
    
    // The canvas allows direct Skia drawing from Lua:
    //   aria.ui.canvas_draw_rect(x, y, w, h, color)
    //   aria.ui.canvas_draw_text(x, y, text, size, color)
    //   aria.ui.canvas_draw_line(x1, y1, x2, y2, color, width)
    //   aria.ui.canvas_draw_circle(x, y, radius, color)
    //   aria.ui.canvas_draw_path(path, color)
};
```

---

## 8. Automation Scripts

### 8.1. Scripted Automation Source

```lua
-- Name: Custom LFO
-- Type: automation
-- Parameters:
--   waveform: { type: enum, values: ["sine","triangle","saw","square"], default: "sine" }
--   rate: { type: float, min: 0.1, max: 20, default: 1 }
--   depth: { type: float, min: 0, max: 1, default: 0.5 }

local phase = 0

function on_parameter_change(id, value)
    if id == "waveform" then waveform = value end
    if id == "rate" then script_rate = value end
    if id == "depth" then depth = value end
end

function on_timer(dt)
    phase = phase + script_rate * dt * 0.001
    if phase > 1 then phase = phase - 1 end
    
    local value = 0.5  -- center
    
    if waveform == "sine" then
        value = 0.5 + 0.5 * math.sin(phase * 2 * math.pi)
    elseif waveform == "triangle" then
        value = 2 * math.abs(phase - 0.5)
    elseif waveform == "saw" then
        value = phase
    elseif waveform == "square" then
        value = phase < 0.5 and 0 or 1
    end
    
    -- Apply depth modulation
    value = 0.5 + (value - 0.5) * depth
    
    return value
end
```

---

## 9. Macro System

### 9.1. Macro Recording

```cpp
class MacroRecorder {
public:
    // Start/stop recording
    void start_recording();
    void stop_recording();
    bool is_recording() const;
    
    // Recorded actions
    struct MacroAction {
        std::string type;         // "parameter_change", "note_add", "transport", etc.
        std::vector<uint8_t> data;  // Serialized action data
        uint64_t timestamp_ms;     // Relative to macro start
    };
    
    // Playback
    void play_macro(const std::vector<MacroAction>& actions);
    
    // Generated script
    std::string generate_lua_script(const std::vector<MacroAction>& actions);
    
    // Recorded actions types:
    // - Transport: play, stop, record, position change
    // - Track: add, remove, mute, solo, volume, pan
    // - Clip: add, remove, edit notes, quantize, transpose
    // - Automation: add point, modify curve
    // - Mixer: fader move, mute, solo, send level
    // - Plugin: parameter change, bypass, preset load
};
```

---

## 10. Script Manager

### 10.1. Script Management

```cpp
class ScriptManager {
public:
    // Install/remove scripts
    bool install_script(const std::string& path);
    bool remove_script(const std::string& name);
    
    // List installed scripts
    std::vector<ScriptInfo> list_scripts(ScriptType type) const;
    std::vector<ScriptInfo> search_scripts(const std::string& query) const;
    
    // Enable/disable
    void enable_script(const std::string& name);
    void disable_script(const std::string& name);
    bool is_enabled(const std::string& name) const;
    
    // Configure script parameters
    void set_parameter(const std::string& script, const std::string& param, double value);
    double get_parameter(const std::string& script, const std::string& param) const;
    
    // Hot reload
    bool hot_reload(const std::string& name);
    
    // Script locations:
    //   User: %APPDATA%/ARIA/scripts/  (Windows)
    //         ~/Library/Application Support/ARIA/scripts/ (macOS)
    //         ~/.config/aria/scripts/ (Linux)
    //   Factory: [install dir]/scripts/
    std::string script_directory() const;
    
    // Script marketplace (future)
    void open_marketplace();

private:
    std::unordered_map<std::string, std::unique_ptr<ScriptInstance>> scripts_;
    std::unordered_set<std::string> disabled_scripts_;
};
```

---

## 11. Security & Sandboxing

### 11.1. Sandbox Limits

```cpp
class LuaSandbox {
public:
    // Memory limit
    static int memory_limit(lua_State* L);
    
    // Execution time limit (instruction count)
    static int instruction_limit(lua_State* L);
    
    // Restricted libraries:
    //   - io.* → REMOVED (file I/O)
    //   - os.* → REMOVED (OS commands)
    //   - load/loadfile → REMOVED (dynamic code loading)
    //   - require → RESTRICTED (only script directory)
    //   - debug.* → RESTRICTED (debug only)
    //
    // Safe libraries:
    //   - string.* → FULL ACCESS
    //   - table.* → FULL ACCESS
    //   - math.* → FULL ACCESS
    //   - coroutine.* → FULL ACCESS (for async)
    //
    // Custom ARIA API:
    //   - aria.* → FULL ACCESS (sandboxed by implementation)
    
    static void apply_sandbox(lua_State* L);
    static bool is_safe_function(const std::string& name);
};
```

### 11.2. Error Handling

```cpp
// Script errors are caught and reported:
// 1. Syntax error → load fails with message
// 2. Runtime error → on_error() hook, script disabled
// 3. Timeout → script killed, logged
// 4. Memory limit → GC forced, if still over → killed
//
// Error dialog shown to user:
//   "Script 'Arpeggiator' error: attempt to index a nil value
//    [Open in Editor] [Disable Script] [Ignore]"
```

---

## 12. API Reference

### 12.1. Public API

```cpp
class ScriptingAPI {
public:
    // Initialize scripting
    bool init(const LuaConfig& config);
    void shutdown();
    
    // Script management
    ScriptManager& scripts();
    
    // Macro recording
    MacroRecorder& macros();
    
    // Execution
    void process_midi_scripts(MidiEventList& events);
    void process_audio_scripts(AudioBlock& block);
    void process_automation_scripts();
    
    // UI
    void render_script_ui(SkiaCanvas* canvas);
    void handle_script_ui_event(const UIEvent& event);
    
    // Diagnostics
    void show_console();
    void show_script_editor(const std::string& name);
};
```

---

## 13. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-SC-001 | Lua Runtime & VM Management | 🔲 Pending |
| RFC-SC-002 | Script Instance Lifecycle | 🔲 Pending |
| RFC-SC-003 | Public API Binding System | 🔲 Pending |
| RFC-SC-004 | MIDI Script Processing | 🔲 Pending |
| RFC-SC-005 | Audio Script Processing | 🔲 Pending |
| RFC-SC-006 | UI Script Canvas & Widgets | 🔲 Pending |
| RFC-SC-007 | Automation Script Sources | 🔲 Pending |
| RFC-SC-008 | Macro Recording & Playback | 🔲 Pending |
| RFC-SC-009 | Macro→Lua Code Generation | 🔲 Pending |
| RFC-SC-010 | Script Manager & Installation | 🔲 Pending |
| RFC-SC-011 | Lua Sandbox (Memory, Time, Libraries) | 🔲 Pending |
| RFC-SC-012 | Hot Reload | 🔲 Pending |
| RFC-SC-013 | Script Error Handling | 🔲 Pending |
| RFC-SC-014 | Script Debugger & Console | 🔲 Pending |
| RFC-SC-015 | Script Marketplace (Future) | 🔲 Pending |
| RFC-SC-016 | Script MIDI Generator Type | 🔲 Pending |
| RFC-SC-017 | Script Audio Generator (Synth) Type | 🔲 Pending |
| RFC-SC-018 | Script Parameter System | 🔲 Pending |

---

## Appendix A: Script Performance

| Script Type | Typical CPU | Max CPU | Notes |
|---|---|---|---|
| MIDI effect (arpeggiator) | 0.01ms | 0.1ms | Per MIDI event |
| MIDI generator (seq) | 0.05ms | 0.2ms | Per timer tick |
| Audio effect (bitcrusher) | 0.1ms | 0.5ms | Per buffer |
| Audio generator (synth) | 0.3ms | 1.0ms | Per buffer |
| UI script (panel) | 0.1ms | 0.5ms | Per frame |
| Automation script | 0.01ms | 0.05ms | Per evaluation |
| Macro playback | 0.001ms | 0.01ms | Per action |

## Appendix B: Script File Locations

```
User scripts:
  Windows: %APPDATA%/ARIA/scripts/
  macOS:   ~/Library/Application Support/ARIA/scripts/
  Linux:   ~/.config/aria/scripts/

Factory scripts (shipped):
  [install_dir]/scripts/

Script data (per-user preferences):
  Windows: %APPDATA%/ARIA/script_data/
  macOS:   ~/Library/Application Support/ARIA/script_data/
  Linux:   ~/.config/aria/script_data/
```
