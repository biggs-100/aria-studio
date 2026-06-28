# ARIA DAW — Public API Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: C API (C++ implementation)
> **Bindings**: Lua, Python, C, C++

---

## Table of Contents

1. [Overview](#1-overview)
2. [API Architecture](#2-api-architecture)
3. [Core API](#3-core-api)
4. [Project API](#4-project-api)
5. [Transport API](#5-transport-api)
6. [Track API](#6-track-api)
7. [Clip API](#7-clip-api)
8. [MIDI API](#8-midi-api)
9. [Audio API](#9-audio-api)
10. [Mixer API](#10-mixer-api)
11. [Plugin API](#11-plugin-api)
12. [Automation API](#12-automation-api)
13. [UI API](#13-ui-api)
14. [File System API](#14-file-system-api)
15. [Scripting API](#15-scripting-api)
16. [Versioning & Stability](#16-versioning--stability)
17. [RFC Index](#17-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Public API defines the stable, versioned interface for interacting with ARIA DAW programmatically. It is the foundation for the Lua scripting system, future plugin SDKs, and external tool integration. Every public function is documented, versioned, and guaranteed stable within a major version.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| API stability | Backwards compatible within major versions |
| Language bindings | C (primary), Lua, Python |
| Thread safety | Documented per function |
| Error handling | Return codes, never exceptions |
| Documentation | 100% function coverage |

### 1.3. API Principles

```
1. Stability: Once public, always public (within major version)
2. Simplicity: Functions do one thing, well
3. Consistency: Naming conventions throughout
4. Discoverability: Related functions grouped by module
5. Error transparency: Every function returns a status code
6. Thread safety: Documented which thread each function can be called from
```

---

## 2. API Architecture

### 2.1. Module Structure

```
aria/                    # Root namespace
├── core/                # Core functions (version, config, etc.)
├── project/             # Project management
├── transport/           # Playback control
├── track/               # Track operations
├── clip/                # Clip operations (audio, MIDI)
├── midi/                # MIDI input/output
├── audio/               # Audio engine access
├── mixer/               # Mixer control
├── plugin/              # Plugin management
├── automation/          # Automation control
├── ui/                  # UI interaction
├── filesystem/          # File browser, sample DB
└── script/              # Script management
```

### 2.2. Naming Convention

```c
// All public functions follow the pattern:
//   aria_<module>_<action>[_<subaction>]

// Examples:
aria_result_t aria_project_open(const char* path);
aria_result_t aria_transport_play();
aria_result_t aria_track_set_volume(aria_track_id track, double db);
aria_track_id aria_track_create(const char* name, aria_track_type type);
```

### 2.3. Error Handling

```c
// All functions return aria_result_t:
typedef enum {
    ARIA_OK                     = 0,
    ARIA_ERROR_GENERIC          = -1,
    ARIA_ERROR_NOT_INITIALIZED  = -2,
    ARIA_ERROR_INVALID_PARAM    = -3,
    ARIA_ERROR_NOT_FOUND        = -4,
    ARIA_ERROR_OUT_OF_MEMORY    = -5,
    ARIA_ERROR_BUSY             = -6,
    ARIA_ERROR_TIMEOUT          = -7,
    ARIA_ERROR_PERMISSION       = -8,
    ARIA_ERROR_UNSUPPORTED      = -9,
    ARIA_ERROR_ALREADY_EXISTS   = -10,
    ARIA_ERROR_DEVICE_ERROR     = -20,
    ARIA_ERROR_AUDIO_GLITCH     = -21,
    ARIA_ERROR_PLUGIN_CRASH     = -30,
    ARIA_ERROR_PLUGIN_NOT_FOUND = -31,
    ARIA_ERROR_LUA_ERROR        = -40,
    ARIA_ERROR_FILE_NOT_FOUND   = -50,
    ARIA_ERROR_FILE_CORRUPT     = -51,
    ARIA_ERROR_FILE_TOO_LARGE   = -52,
} aria_result_t;

// Error details:
const char* aria_error_string(aria_result_t result);
// Returns a human-readable error message for the given code.
```

---

## 3. Core API

### 3.1. Lifecycle

```c
// Initialize ARIA engine
aria_result_t aria_init(const aria_config* config);

// Shutdown ARIA engine
aria_result_t aria_shutdown();

// Check if initialized
bool aria_is_initialized();

// Get version info
typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    const char* build;          // "release", "beta", "nightly"
    const char* commit_hash;
} aria_version;
aria_version aria_get_version();
```

### 3.2. Configuration

```c
typedef struct {
    uint32_t sample_rate;               // Default: 48000
    uint32_t buffer_size;               // Default: 128
    const char* audio_device_id;        // NULL = default
    const char* midi_input_device_id;   // NULL = none
    const char* midi_output_device_id;  // NULL = none
    uint32_t max_undo_steps;            // 0 = unlimited
    bool enable_gpu_acceleration;       // Default: true
    bool enable_ai_features;            // Default: true
    bool enable_scripting;              // Default: true
    bool enable_networking;             // Default: false
    const char* user_data_path;         // NULL = default
} aria_config;

// Get/set individual config values at runtime
aria_result_t aria_config_set_string(const char* key, const char* value);
aria_result_t aria_config_set_int(const char* key, int32_t value);
aria_result_t aria_config_set_float(const char* key, double value);
aria_result_t aria_config_get_string(const char* key, char* buffer, uint32_t buffer_size);
int32_t       aria_config_get_int(const char* key, int32_t default_value);
double        aria_config_get_float(const char* key, double default_value);
```

---

## 4. Project API

### 4.1. Project Management

```c
// Create new project
aria_result_t aria_project_create(const char* path, const char* name);

// Open existing project
aria_result_t aria_project_open(const char* path);

// Save current project
aria_result_t aria_project_save();
aria_result_t aria_project_save_as(const char* path);

// Close current project
aria_result_t aria_project_close();

// Project info
typedef struct {
    char name[256];
    char path[1024];
    char author[256];
    double tempo;
    uint32_t time_signature_num;
    uint32_t time_signature_den;
    uint32_t sample_rate;
    uint64_t duration_ppqn;
    bool modified;
} aria_project_info;
aria_result_t aria_project_get_info(aria_project_info* info);
aria_result_t aria_project_set_author(const char* author);

// Auto-save
void aria_project_enable_autosave(bool enable);
void aria_project_trigger_autosave();
```

---

## 5. Transport API

### 5.1. Transport Control

```c
// Transport state
typedef enum {
    ARIA_TRANSPORT_STOPPED,
    ARIA_TRANSPORT_PLAYING,
    ARIA_TRANSPORT_RECORDING,
    ARIA_TRANSPORT_PAUSED,
} aria_transport_state;

// Control
aria_result_t aria_transport_play();
aria_result_t aria_transport_stop();
aria_result_t aria_transport_record();
aria_result_t aria_transport_pause();
aria_result_t aria_transport_toggle_play();

// Position
aria_result_t aria_transport_set_position(uint64_t ppqn);
uint64_t      aria_transport_get_position();
double        aria_transport_get_time_seconds();
aria_result_t aria_transport_get_bar_beat(int* bar, int* beat, int* sixteenth);

// Tempo
aria_result_t aria_transport_set_tempo(double bpm);
double        aria_transport_get_tempo();

// Time signature
aria_result_t aria_transport_set_time_signature(int numerator, int denominator);
aria_result_t aria_transport_get_time_signature(int* numerator, int* denominator);

// Loop
void aria_transport_set_loop(bool enabled);
bool aria_transport_get_loop();
void aria_transport_set_loop_range(uint64_t start_ppqn, uint64_t end_ppqn);
void aria_transport_get_loop_range(uint64_t* start_ppqn, uint64_t* end_ppqn);

// State
aria_transport_state aria_transport_get_state();
bool aria_transport_is_playing();
bool aria_transport_is_recording();
```

---

## 6. Track API

### 6.1. Track Management

```c
// Track types
typedef enum {
    ARIA_TRACK_AUDIO,
    ARIA_TRACK_MIDI,
    ARIA_TRACK_GROUP,
    ARIA_TRACK_VCA,
    ARIA_TRACK_RETURN,
    ARIA_TRACK_MASTER,
} aria_track_type;

typedef uint64_t aria_track_id;
#define ARIA_TRACK_INVALID 0

// Create/delete
aria_track_id aria_track_create(const char* name, aria_track_type type);
aria_result_t aria_track_delete(aria_track_id track);

// Queries
uint32_t      aria_track_count();
aria_track_id aria_track_get(uint32_t index);
aria_track_id aria_track_find_by_name(const char* name);
aria_track_id aria_track_get_master();

// Properties
aria_result_t aria_track_set_name(aria_track_id track, const char* name);
aria_result_t aria_track_get_name(aria_track_id track, char* buffer, uint32_t size);
aria_track_type aria_track_get_type(aria_track_id track);

// State
aria_result_t aria_track_set_mute(aria_track_id track, bool mute);
bool aria_track_get_mute(aria_track_id track);
aria_result_t aria_track_set_solo(aria_track_id track, bool solo);
bool aria_track_get_solo(aria_track_id track);
aria_result_t aria_track_set_arm(aria_track_id track, bool arm);
bool aria_track_get_arm(aria_track_id track);

// Volume/Pan
aria_result_t aria_track_set_volume(aria_track_id track, double db);
double        aria_track_get_volume(aria_track_id track);
aria_result_t aria_track_set_pan(aria_track_id track, double pan);
double        aria_track_get_pan(aria_track_id track);

// Input/Output
aria_result_t aria_track_set_input(aria_track_id track, const char* device, uint32_t channel);
aria_result_t aria_track_set_output(aria_track_id track, aria_track_id target);

// Group/VCA
aria_result_t aria_track_set_group(aria_track_id track, aria_track_id group);
aria_track_id aria_track_get_group(aria_track_id track);
aria_result_t aria_track_set_vca(aria_track_id track, aria_track_id vca);
aria_track_id aria_track_get_vca(aria_track_id track);

// Color
aria_result_t aria_track_set_color(aria_track_id track, uint8_t r, uint8_t g, uint8_t b);
aria_result_t aria_track_get_color(aria_track_id track, uint8_t* r, uint8_t* g, uint8_t* b);
```

---

## 7. Clip API

### 7.1. Clip Management

```c
typedef uint64_t aria_clip_id;

// Create midi clip on track
aria_clip_id aria_clip_create_midi(aria_track_id track, uint64_t start_ppqn, uint64_t length_ppqn);

// Create audio clip on track (references a file)
aria_clip_id aria_clip_create_audio(aria_track_id track, uint64_t start_ppqn,
                                     const char* file_path);

// Delete clip
aria_result_t aria_clip_delete(aria_clip_id clip);

// Clip properties
aria_result_t aria_clip_set_start(aria_clip_id clip, uint64_t ppqn);
uint64_t      aria_clip_get_start(aria_clip_id clip);
aria_result_t aria_clip_set_length(aria_clip_id clip, uint64_t ppqn);
uint64_t      aria_clip_get_length(aria_clip_id clip);
aria_result_t aria_clip_set_mute(aria_clip_id clip, bool mute);
bool          aria_clip_get_mute(aria_clip_id clip);
aria_result_t aria_clip_set_gain(aria_clip_id clip, double db);
double        aria_clip_get_gain(aria_clip_id clip);

// MIDI note operations
aria_result_t aria_clip_add_note(aria_clip_id clip, uint8_t pitch,
                                  uint64_t start_ppqn, uint64_t duration_ppqn,
                                  uint8_t velocity);
aria_result_t aria_clip_remove_note(aria_clip_id clip, uint32_t note_index);
uint32_t      aria_clip_get_note_count(aria_clip_id clip);

// Bulk operations
aria_result_t aria_clip_quantize(aria_clip_id clip, uint32_t grid_ppqn, double strength);
aria_result_t aria_clip_transpose(aria_clip_id clip, int8_t semitones);
aria_result_t aria_clip_humanize(aria_clip_id clip, double timing, double velocity);
```

---

## 8. MIDI API

### 8.1. MIDI I/O

```c
// MIDI event types
typedef enum {
    ARIA_MIDI_NOTE_ON,
    ARIA_MIDI_NOTE_OFF,
    ARIA_MIDI_CC,
    ARIA_MIDI_PITCH_BEND,
    ARIA_MIDI_CHANNEL_PRESSURE,
    ARIA_MIDI_POLY_PRESSURE,
    ARIA_MIDI_PROGRAM_CHANGE,
    ARIA_MIDI_SYSEX,
    ARIA_MIDI_CLOCK,
    ARIA_MIDI_START,
    ARIA_MIDI_STOP,
    ARIA_MIDI_CONTINUE,
} aria_midi_event_type;

typedef struct {
    aria_midi_event_type type;
    uint8_t channel;
    uint8_t data1;          // Note, CC number, etc.
    uint8_t data2;          // Velocity, CC value, etc.
    const uint8_t* sysex_data;
    uint32_t sysex_size;
} aria_midi_event;

// Send MIDI
aria_result_t aria_midi_send(const aria_midi_event* event);
aria_result_t aria_midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
aria_result_t aria_midi_send_note_off(uint8_t channel, uint8_t note);
aria_result_t aria_midi_send_cc(uint8_t channel, uint8_t cc, uint8_t value);
aria_result_t aria_midi_send_pitch_bend(uint8_t channel, int16_t value);
aria_result_t aria_midi_send_sysex(const uint8_t* data, uint32_t size);

// Device enumeration
uint32_t aria_midi_get_input_count();
uint32_t aria_midi_get_output_count();
aria_result_t aria_midi_get_input_name(uint32_t index, char* buffer, uint32_t size);
aria_result_t aria_midi_get_output_name(uint32_t index, char* buffer, uint32_t size);

// Callbacks
typedef void (*aria_midi_input_callback)(const aria_midi_event* event, void* user_data);
void aria_midi_set_input_callback(aria_midi_input_callback callback, void* user_data);

// MIDI learn
void aria_midi_start_learn();
void aria_midi_stop_learn();
bool aria_midi_is_learning();
```

---

## 9. Audio API

### 9.1. Audio Engine

```c
// Engine info
typedef struct {
    double sample_rate;
    uint32_t buffer_size;
    uint32_t input_channels;
    uint32_t output_channels;
    double latency_ms;
    uint32_t xrun_count;
    double cpu_usage;
} aria_audio_info;
aria_result_t aria_audio_get_info(aria_audio_info* info);

// Device management
uint32_t aria_audio_get_device_count();
aria_result_t aria_audio_get_device_name(uint32_t index, char* buffer, uint32_t size);
aria_result_t aria_audio_get_device_info(uint32_t index, aria_audio_device_info* info);

// Metering
double aria_audio_get_track_peak(aria_track_id track, uint32_t channel);
double aria_audio_get_track_rms(aria_track_id track, uint32_t channel);

// Audio input monitoring
void aria_audio_set_monitor(bool enabled);
```

---

## 10. Mixer API

### 10.1. Mixer Control

```c
// Channel
aria_result_t aria_mixer_set_volume(aria_track_id track, double db);
double        aria_mixer_get_volume(aria_track_id track);
aria_result_t aria_mixer_set_pan(aria_track_id track, double pan);
double        aria_mixer_get_pan(aria_track_id track);

// Sends
typedef uint32_t aria_send_id;
aria_result_t aria_mixer_add_send(aria_track_id source, aria_track_id destination,
                                   double level_db, bool pre_fader);
aria_result_t aria_mixer_remove_send(aria_track_id source, uint32_t send_index);
aria_result_t aria_mixer_set_send_level(aria_track_id source, uint32_t send_index, double level_db);

// FX
aria_result_t aria_mixer_add_fx(aria_track_id track, const char* plugin_id);
aria_result_t aria_mixer_remove_fx(aria_track_id track, uint32_t fx_index);
aria_result_t aria_mixer_set_fx_bypass(aria_track_id track, uint32_t fx_index, bool bypass);

// Metering
double aria_mixer_get_channel_peak(aria_track_id track);
double aria_mixer_get_channel_rms(aria_track_id track);
```

---

## 11. Plugin API

### 11.1. Plugin Management

```c
// Plugin discovery
uint32_t aria_plugin_get_count();
aria_result_t aria_plugin_get_info(uint32_t index, aria_plugin_info* info);

typedef struct {
    char id[128];
    char name[256];
    char vendor[256];
    char format[16];        // "CLAP", "VST3", "AU", "LV2"
    char category[64];
    uint32_t num_inputs;
    uint32_t num_outputs;
    bool has_editor;
    bool is_synth;
} aria_plugin_info;

// Plugin parameters
uint32_t aria_plugin_get_parameter_count(aria_track_id track, uint32_t fx_index);
aria_result_t aria_plugin_get_parameter_name(aria_track_id track, uint32_t fx_index,
                                              uint32_t param_index, char* buffer, uint32_t size);
aria_result_t aria_plugin_set_parameter(aria_track_id track, uint32_t fx_index,
                                         uint32_t param_index, double value);
double        aria_plugin_get_parameter(aria_track_id track, uint32_t fx_index,
                                         uint32_t param_index);

// Presets
aria_result_t aria_plugin_load_preset(aria_track_id track, uint32_t fx_index,
                                       const char* preset_path);
aria_result_t aria_plugin_save_preset(aria_track_id track, uint32_t fx_index,
                                       const char* preset_path);
```

---

## 12. Automation API

### 12.1. Automation Control

```c
typedef uint64_t aria_automation_lane_id;

// Create automation lane
aria_automation_lane_id aria_automation_create_lane(aria_track_id track, const char* parameter_id);
aria_result_t aria_automation_delete_lane(aria_automation_lane_id lane);

// Points
aria_result_t aria_automation_add_point(aria_automation_lane_id lane,
                                         uint64_t ppqn, double value);
aria_result_t aria_automation_remove_point(aria_automation_lane_id lane, uint32_t index);
uint32_t      aria_automation_get_point_count(aria_automation_lane_id lane);

// Recording
void aria_automation_start_record(aria_automation_lane_id lane);
void aria_automation_stop_record();

// Modulation
aria_result_t aria_automation_set_modulation(aria_track_id track, const char* parameter_id,
                                              const char* source_type, double amount);
```

---

## 13. UI API

### 13.1. UI Interaction

```c
// Focus
aria_result_t aria_ui_focus_track(aria_track_id track);
aria_result_t aria_ui_focus_clip(aria_clip_id clip);

// View switching
typedef enum {
    ARIA_VIEW_ARRANGEMENT,
    ARIA_VIEW_SESSION,
    ARIA_VIEW_MIXER,
    ARIA_VIEW_PIANO_ROLL,
    ARIA_VIEW_BROWSER,
} aria_view_type;
aria_result_t aria_ui_switch_to_view(aria_view_type view);

// Notifications
aria_result_t aria_ui_show_notification(const char* message, int duration_ms);
aria_result_t aria_ui_show_dialog(const char* title, const char* message);
```

---

## 14. File System API

### 14.1. File Browser Access

```c
// Search
typedef struct {
    char name[256];
    char path[1024];
    char category[64];
    double bpm;
    char key[8];
    double duration;
    int rating;
} aria_sample_info;

uint32_t aria_filesystem_search(const char* query, aria_sample_info* results, uint32_t max_results);

// Watched folders
uint32_t aria_filesystem_get_watched_folder_count();
aria_result_t aria_filesystem_get_watched_folder(uint32_t index, char* buffer, uint32_t size);
aria_result_t aria_filesystem_add_watched_folder(const char* path);
aria_result_t aria_filesystem_remove_watched_folder(const char* path);
```

---

## 15. Scripting API

### 15.1. Script Management

```c
// Install/list scripts
aria_result_t aria_script_install(const char* path);
aria_result_t aria_script_uninstall(const char* name);
uint32_t      aria_script_get_count();
aria_result_t aria_script_get_name(uint32_t index, char* buffer, uint32_t size);

// Enable/disable
aria_result_t aria_script_enable(const char* name);
aria_result_t aria_script_disable(const char* name);
bool          aria_script_is_enabled(const char* name);

// Parameters
aria_result_t aria_script_set_parameter(const char* script, const char* param, double value);
double        aria_script_get_parameter(const char* script, const char* param);

// Console output
void aria_script_log(const char* message);
```

---

## 16. Versioning & Stability

### 16.1. API Version Policy

```
ARIA API Versioning:

API version follows the application version:
  ARIA v1.0.0 → API v1.0
  
Major version:
  - Breaking changes allowed
  - Removal of deprecated functions
  - Migration guide required
  
Minor version:
  - Additions only (no removals)
  - New functions, new types
  - Backwards compatible
  
Patch version:
  - Bug fixes only
  - No API changes
  - Documentation fixes

Deprecation cycle:
  vN.0: Function marked deprecated (documentation warning)
  vN+1.0: Deprecated function still compiles with warning
  vN+2.0: Deprecated function removed
```

### 16.2. Deprecation Macros

```c
// API functions use deprecation macros:
#define ARIA_DEPRECATED(version) __attribute__((deprecated("Deprecated since " #version)))

ARIA_DEPRECATED("2.0")
aria_result_t aria_old_function();
```

---

## 17. RFC Index

| RFC | Module | Status |
|---|---|---|
| RFC-API-001 | API Architecture & Error Handling | 🔲 Pending |
| RFC-API-002 | Core API (init, config, version) | 🔲 Pending |
| RFC-API-003 | Project API (create, open, save) | 🔲 Pending |
| RFC-API-004 | Transport API (play, stop, record, tempo) | 🔲 Pending |
| RFC-API-005 | Track API (create, properties, routing) | 🔲 Pending |
| RFC-API-006 | Clip API (MIDI/audio notes) | 🔲 Pending |
| RFC-API-007 | MIDI I/O API | 🔲 Pending |
| RFC-API-008 | Audio Engine API | 🔲 Pending |
| RFC-API-009 | Mixer API (vol, pan, sends, FX) | 🔲 Pending |
| RFC-API-010 | Plugin API (discovery, params, presets) | 🔲 Pending |
| RFC-API-011 | Automation API (lanes, points, mod) | 🔲 Pending |
| RFC-API-012 | UI API (focus, views, notifications) | 🔲 Pending |
| RFC-API-013 | File System API (search, watch) | 🔲 Pending |
| RFC-API-014 | Scripting API (install, params) | 🔲 Pending |
| RFC-API-015 | C API Binding Generator | 🔲 Pending |
| RFC-API-016 | Python Bindings | 🔲 Pending |
| RFC-API-017 | API Versioning & Deprecation | 🔲 Pending |
| RFC-API-018 | API Documentation Generator | 🔲 Pending |
| RFC-API-019 | Thread Safety Documentation | 🔲 Pending |
| RFC-API-020 | Async API Patterns | 🔲 Pending |

---

## Appendix A: API Coverage Summary

| Module | Functions | Status |
|---|---|---|
| Core | 10 | ✅ Specified |
| Project | 10 | ✅ Specified |
| Transport | 20 | ✅ Specified |
| Track | 35 | ✅ Specified |
| Clip | 18 | ✅ Specified |
| MIDI | 15 | ✅ Specified |
| Audio | 8 | ✅ Specified |
| Mixer | 12 | ✅ Specified |
| Plugin | 10 | ✅ Specified |
| Automation | 8 | ✅ Specified |
| UI | 5 | ✅ Specified |
| File System | 5 | ✅ Specified |
| Scripting | 8 | ✅ Specified |
| **Total** | **164** | |

## Appendix B: Thread Safety

```
Thread Safety Legend:
  ✓ = Thread-safe (any thread)
  M = Main thread only
  A = Audio thread only (real-time safe)
  B = Background thread only
  ✗ = Not thread-safe (must be synchronized by caller)

Key:
aria_init()                    → M
aria_transport_play()          → M
aria_track_set_volume()        → M
aria_track_get_volume()        → ✓
aria_midi_send()               → A
aria_mixer_get_channel_peak()  → ✓
```
