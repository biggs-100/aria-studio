# ARIA DAW — Plugin Host Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: C++20/23
> **Formats**: CLAP (native), VST3, AU (macOS), LV2 (Linux)

---

## 1. Overview

### 1.1. Purpose

The Plugin Host manages all third-party and native audio plugins in ARIA DAW. It provides a unified interface over multiple plugin formats, handles plugin lifecycle, sandboxing, parameter management, preset browsing, and crash isolation.

### 1.2. Native Plugin Strategy

**CLAP is the native format.** All built-in ARIA plugins (synthesizers, effects, utilities) are CLAP plugins. They load through the same pipeline as third-party CLAP plugins. There is no "internal fast path." This ensures:

- Built-in plugins can be used in any CLAP-compatible DAW
- No proprietary plugin format to maintain
- Plugin developers have a reference implementation
- Sandboxing applies equally to all plugins

### 1.3. Design Goals

| Goal | Target |
|---|---|
| Format support | CLAP (native), VST3, AU, LV2 |
| Sandboxing | Optional out-of-process crash isolation |
| PDC | Automatic, sample-accurate |
| Plugin scan | Incremental, < 3s on startup |
| Parameter count | Unlimited per plugin |
| Preset format | CLAP preset + custom metadata |

---

## 2. Plugin Abstraction Layer

### 2.1. Unified Plugin Interface

```cpp
class AudioPlugin {
public:
    virtual ~AudioPlugin() = default;

    // Lifecycle
    virtual bool init() = 0;
    virtual bool activate(double sample_rate, uint32_t min_frames,
                           uint32_t max_frames) = 0;
    virtual void deactivate() = 0;
    virtual void destroy() = 0;

    // Identification
    virtual PluginID id() const = 0;
    virtual std::string name() const = 0;
    virtual std::string vendor() const = 0;
    virtual std::string format() const = 0;  // "CLAP", "VST3", "AU", "LV2"
    virtual std::string version() const = 0;
    virtual PluginCategory category() const = 0;  // Synth, Effect, Analyzer, etc.

    // Audio processing
    virtual void process(const ProcessContext& ctx) = 0;
    
    // MIDI events
    virtual void set_midi_events(const MidiEventList& events) = 0;
    
    // Parameters
    virtual uint32_t parameter_count() const = 0;
    virtual ParameterInfo parameter_info(uint32_t index) const = 0;
    virtual double get_parameter_value(ParamID id) const = 0;
    virtual void set_parameter_value(ParamID id, double value) = 0;
    virtual void begin_edit(ParamID id) = 0;
    virtual void end_edit(ParamID id) = 0;
    
    // State
    virtual bool save_state(std::vector<uint8_t>& data) const = 0;
    virtual bool load_state(const std::vector<uint8_t>& data) = 0;
    
    // Preset
    virtual bool load_preset(const std::string& path) = 0;
    virtual bool save_preset(const std::string& path) = 0;
    
    // Latency
    virtual uint32_t latency() const = 0;
    
    // GUI
    virtual bool has_editor() const = 0;
    virtual bool create_editor(void* parent_window) = 0;
    virtual bool destroy_editor() = 0;
    virtual void set_editor_scale(double scale) = 0;
    
    // Bypass
    virtual void set_bypass(bool bypass) = 0;
    virtual bool is_bypassed() const = 0;
    
    // Tail
    virtual uint32_t tail_samples() const = 0;  // Silence after input stops
    
    // Audio I/O config
    virtual AudioIOConfiguration audio_io_config() const = 0;
};

struct ProcessContext {
    float** inputs;           // Input audio buffers
    float** outputs;          // Output audio buffers
    uint32_t input_channels;
    uint32_t output_channels;
    uint32_t frames;          // Buffer size
    uint64_t sample_position; // Global sample position
    double sample_rate;
    bool is_playing;
    bool is_recording;
    double tempo;
    double time_signature_numerator;
    double time_signature_denominator;
    bool transport_changed;
};

enum class PluginCategory {
    Synth,           // Instrument/synthesizer
    Effect,          // Audio effect
    Analyzer,        // Meter/analyzer
    MIDI,            // MIDI effect
    Utility,         // Utility (gain, meter, etc.)
    Generator,       // Audio generator
    Other
};
```

### 2.2. Plugin Factory

```cpp
class PluginFactory {
public:
    static PluginFactory& instance();
    
    // Scan plugins (incremental)
    void scan_plugins(const std::vector<std::string>& paths,
                       ScanMode mode = ScanMode::Incremental);
    
    enum class ScanMode {
        Full,           // Full rescan
        Incremental,    // Only new/changed
        Quick,          // Use cache only
        Background      // Background priority scanning
    };
    
    // Create plugin instance
    std::unique_ptr<AudioPlugin> create_plugin(const PluginID& id);
    
    // Discovered plugins
    const std::vector<PluginDescriptor>& available_plugins() const;
    const PluginDescriptor* find_plugin(const PluginID& id) const;
    
    // Scan progress
    double scan_progress() const;
    bool is_scanning() const;
    
    // Events
    std::function<void(const PluginDescriptor&)> on_plugin_discovered;

private:
    // Format-specific scanners
    std::vector<std::unique_ptr<FormatScanner>> scanners_;
    std::unordered_map<PluginID, PluginDescriptor> plugin_registry_;
    
    // Scan state
    std::atomic<bool> scanning_{false};
    std::atomic<double> progress_{0.0};
};

struct PluginDescriptor {
    PluginID id;
    std::string name;
    std::string vendor;
    std::string format;         // "CLAP", "VST3", "AU", "LV2"
    std::string version;
    std::string path;            // File path
    PluginCategory category;
    uint32_t num_inputs;
    uint32_t num_outputs;
    bool has_editor;
    bool is_synth;
    std::vector<std::string> features;  // Tags like "equalizer", "reverb", "synth"
    uint64_t last_modified;      // File timestamp for incremental scanning
};
```

---

## 3. CLAP Host Implementation

### 3.1. CLAP Host Architecture

```cpp
class CLAPHost : public AudioPlugin {
public:
    CLAPHost(const PluginDescriptor& desc);
    ~CLAPHost();

    // AudioPlugin interface implementation
    bool init() override;
    bool activate(double sample_rate, uint32_t min_frames,
                   uint32_t max_frames) override;
    void deactivate() override;
    void destroy() override;
    
    void process(const ProcessContext& ctx) override;
    void set_midi_events(const MidiEventList& events) override;
    
    uint32_t parameter_count() const override;
    ParameterInfo parameter_info(uint32_t index) const override;
    double get_parameter_value(ParamID id) const override;
    void set_parameter_value(ParamID id, double value) override;
    
    bool save_state(std::vector<uint8_t>& data) const override;
    bool load_state(const std::vector<uint8_t>& data) override;
    
    uint32_t latency() const override;
    
    bool has_editor() const override;
    bool create_editor(void* parent_window) override;
    bool destroy_editor() override;
    
    // ... remaining interface methods

private:
    // CLAP plugin library handle
    void* library_handle_ = nullptr;
    
    // CLAP plugin entry point
    const clap_plugin_entry* entry_ = nullptr;
    const clap_plugin* plugin_ = nullptr;
    const clap_plugin_factory* factory_ = nullptr;
    
    // CLAP host structure
    clap_host host_;
    clap_host_audio_ports host_audio_;
    clap_host_params host_params_;
    clap_host_latency host_latency_;
    clap_host_note_ports host_notes_;
    clap_host_state host_state_;
    clap_host_gui host_gui_;
    clap_host_timer_support host_timers_;
    
    // Plugin callbacks (static functions)
    static void host_init();
    static bool host_callback(const char* name, const void* data, uint32_t size);
    
    // Extensions
    void load_extensions();
    void init_clap_host_struct();
};
```

### 3.2. CLAP Extensions Supported

| Extension | Purpose | Required |
|---|---|---|
| `clap_audio_ports` | Audio I/O configuration | Yes |
| `clap_audio_ports_config` | Predefined I/O configs | Optional |
| `clap_note_ports` | MIDI/note input | Yes |
| `clap_params` | Parameter management | Yes |
| `clap_latency` | Plugin latency reporting | Yes |
| `clap_tail` | Tail samples (reverb tail) | Yes |
| `clap_state` | Save/load plugin state | Yes |
| `clap_preset` | Preset load/save | Yes |
| `clap_gui` | Plugin editor window | Optional |
| `clap_timer_support` | Timer callbacks | Optional |
| `clap_thread_pool` | Thread pool for plugins | Optional |
| `clap_render` | Offline rendering mode | Optional |
| `clap_track_info` | Track color/name to plugin | Optional |
| `clap_voice_info` | Voice/note polyphony info | Optional |
| `clap_voice` | Per-voice processing | Optional |
| `clap_mpe` | MPE support | Yes |

### 3.3. Native CLAP Plugin Factory

```cpp
// ARIA's built-in plugins register through CLAP's plugin factory
// exactly like third-party plugins.

class NativeCLAPFactory {
public:
    static NativeCLAPFactory& instance();
    
    // Register a built-in CLAP plugin
    void register_plugin(const clap_plugin_descriptor& desc,
                         std::function<clap_plugin*(const clap_host*)> factory);
    
    // Built-in plugin entries
    static const clap_plugin_entry entry;
    
    // List of built-in plugins
    static const std::vector<const clap_plugin_descriptor*>& descriptors();

private:
    std::vector<RegisteredPlugin> plugins_;
    
    // CLAP entry points
    static bool init_callback(const char* path);
    static void deinit_callback();
    static uint32_t get_plugin_count_callback();
    static const clap_plugin_descriptor* get_plugin_descriptor_callback(uint32_t index);
    static const clap_plugin* create_plugin_callback(const clap_host* host,
                                                       const char* plugin_id);
};

// Built-in plugins register on startup:
REGISTER_CLAP_PLUGIN(ARIA_EQ, "aria.eq", "ARIA EQ");
REGISTER_CLAP_PLUGIN(ARIA_Compressor, "aria.compressor", "ARIA Compressor");
REGISTER_CLAP_PLUGIN(ARIA_Reverb, "aria.reverb", "ARIA Reverb");
REGISTER_CLAP_PLUGIN(ARIA_Delay, "aria.delay", "ARIA Delay");
REGISTER_CLAP_PLUGIN(ARIA_Synth, "aria.synth", "ARIA Synthesizer");
```

---

## 4. VST3 Host Implementation

### 4.1. VST3 Host Architecture

```cpp
class VST3Host : public AudioPlugin {
public:
    VST3Host(const PluginDescriptor& desc);
    ~VST3Host();
    
    bool init() override;
    bool activate(double sample_rate, uint32_t min_frames,
                   uint32_t max_frames) override;
    void deactivate() override;
    // ... AudioPlugin interface
    
private:
    // Module
    Steinberg::Vst::Module* module_ = nullptr;
    Steinberg::IPluginFactory* factory_ = nullptr;
    Steinberg::Vst::IEditController* controller_ = nullptr;
    Steinberg::Vst::IEditController2* controller2_ = nullptr;
    Steinberg::Vst::IComponent* component_ = nullptr;
    Steinberg::Vst::IAudioProcessor* processor_ = nullptr;
    Steinberg::Vst::IUnitInfo* unit_info_ = nullptr;
    Steinberg::Vst::IParameterChanges* param_changes_ = nullptr;
    
    // VST3 specific
    Steinberg::FUnknownPtr<Steinberg::Vst::IEditController> edit_controller_;
    Steinberg::TBool use_parameter_queues_ = false;
    
    // Parameter update queues (for sample-accurate automation)
    struct ParameterQueue {
        std::vector<ParameterUpdate> changes;
        uint32_t current_index = 0;
    };
    std::unordered_map<ParamID, ParameterQueue> param_queues_;
    
    // Bus info
    uint32_t audio_input_busses_ = 0;
    uint32_t audio_output_busses_ = 0;
    uint32_t event_input_busses_ = 0;
};
```

---

## 5. AU Host Implementation (macOS)

### 5.1. AU Host Architecture

```cpp
// macOS only — implemented in Objective-C++ (.mm)

class AUHost : public AudioPlugin {
public:
    AUHost(const PluginDescriptor& desc);
    ~AUHost();
    
    bool init() override;
    // ... AudioPlugin interface
    
private:
    // CoreAudio types
    AudioUnit audio_unit_ = nullptr;
    AudioComponentInstance component_instance_ = nullptr;
    AudioComponent component_ = nullptr;
    AUAudioUnit* audio_unit_wrapper_ = nullptr;  // V3 API
    
    // AU specific
    AUEventListenerRef event_listener_ = nullptr;
    
    // Audio unit description
    AudioComponentDescription component_desc_;
    
    // Parameter management
    struct AUParameterState {
        AUParameterAddress address;
        AudioUnitParameterValue value;
    };
    std::unordered_map<AUParameterAddress, AUParameterState> parameters_;
    
    // AU v2 vs v3
    bool use_v3_api_ = false;
};
```

---

## 6. LV2 Host Implementation (Linux)

```cpp
class LV2Host : public AudioPlugin {
public:
    LV2Host(const PluginDescriptor& desc);
    ~LV2Host();
    
    bool init() override;
    // ... AudioPlugin interface
    
private:
    // Lilv library wrappers
    LilvWorld* world_ = nullptr;
    const LilvPlugin* plugin_ = nullptr;
    LilvInstance* instance_ = nullptr;
    
    // LV2 features
    LV2_Feature features_[32];
    LV2_URID_Map urid_map_;
    LV2_URID_Unmap urid_unmap_;
    
    // Ports
    struct Port {
        uint32_t index;
        float* data;
        bool is_input;
        bool is_audio;
        bool is_cv;
        bool is_control;
        LV2_URID symbol;
    };
    std::vector<Port> ports_;
    
    // Worker/schedule (for real-time safety)
    LV2_Worker_Schedule worker_schedule_;
};
```

---

## 7. Plugin Sandboxing

### 7.1. Sandbox Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                        ARIA Main Process                             │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │                     Plugin Host                                │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │  │
│  │  │Trusted   │  │Normal    │  │Untrusted │  │Unknown       │  │  │
│  │  │(whitelist)│  │(standard)│  │(sandbox) │  │(out-of-proc) │  │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────────┘  │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

### 7.2. Sandbox Levels

| Level | Trust | Execution | Crash Recovery | Default |
|---|---|---|---|---|
| **0** | Full trust | In-process, same thread | Not isolated | Native ARIA plugins, whitelisted |
| **1** | Known vendor | In-process, separate thread | Thread restart | Signed VST3/CLAP |
| **2** | Untrusted | Out-of-process, IPC | Process restart | Unsigned plugins |
| **3** | Unknown | Out-of-process + limited API | Process restart | First-use plugins |

### 7.3. Out-of-Process Sandbox

```cpp
class OutOfProcessPlugin : public AudioPlugin {
public:
    OutOfProcessPlugin(std::unique_ptr<AudioPlugin> real_plugin);
    ~OutOfProcessPlugin();
    
    // All calls go through shared memory / IPC
    bool init() override;
    void process(const ProcessContext& ctx) override;
    // ... all methods are proxied
    
private:
    // Subprocess management
    PluginProcess process_;
    
    // Shared memory
    SharedMemoryRegion shared_mem_;
    
    // IPC channels
    IPCChannel control_channel_;     // Parameter changes, state
    IPCChannel audio_channel_;       // Audio buffer pointers
    
    // Heartbeat
    std::atomic<bool> process_alive_{true};
    std::thread watchdog_thread_;
    
    // Crash handling
    void on_process_crash();
    void restart_process();
    bool try_recover_state();
};
```

### 7.4. Plugin Blacklist

```cpp
class PluginBlacklist {
public:
    // Mark plugin as crashed
    void report_crash(const PluginID& id, const std::string& reason);
    
    // Check if plugin is blacklisted
    bool is_blacklisted(const PluginID& id) const;
    
    // Blacklist levels
    enum class Level {
        Warning,        // Show warning on load
        Disabled,       // Skip in scan
        Banned          // Never load (known problematic)
    };
    
    // Remove from blacklist
    void clear_entry(const PluginID& id);
    void clear_all();
    
    // Persistent storage
    void save(const std::string& path);
    void load(const std::string& path);

private:
    struct Entry {
        Level level;
        std::string reason;
        uint32_t crash_count;
        uint64_t last_crash_time;
    };
    std::unordered_map<PluginID, Entry> blacklist_;
};
```

---

## 8. Parameter System

### 8.1. Parameter Abstraction

```cpp
struct ParameterInfo {
    ParamID id;                    // Unique parameter ID
    std::string name;              // Human-readable name
    std::string short_name;        // Abbreviated (for UI)
    std::string unit;              // "Hz", "dB", "%", etc.
    
    double min_value;              // 0.0 typically
    double max_value;              // 1.0 typically
    double default_value;          // Default
    double step_count;             // 0 = continuous
    
    bool is_automation;            // Can be automated
    bool is_modulation;            // Can be modulated
    bool is_bypass;                // Bypass parameter
    bool is_hidden;                // Hidden from UI
    
    std::vector<std::string> enum_values;  // For stepped parameters
};

class ParameterManager {
public:
    void register_parameter(const ParameterInfo& info);
    void unregister_parameter(ParamID id);
    
    // Value access (thread-safe)
    void set_value(ParamID id, double value);
    double get_value(ParamID id) const;
    
    // Automation
    void begin_edit(ParamID id);
    void perform_edit(ParamID id, double value);
    void end_edit(ParamID id);
    
    // Modulation (automation stack)
    void set_modulation(ParamID id, double amount, ModulationSource source);
    double effective_value(ParamID id) const;  // base + modulation
    
    // Parameter info
    const ParameterInfo* info(ParamID id) const;
    uint32_t parameter_count() const;
    ParamID parameter_at(uint32_t index) const;
    
    // Text conversion
    std::string value_to_text(ParamID id, double value) const;
    double text_to_value(ParamID id, const std::string& text) const;
    
private:
    std::unordered_map<ParamID, ParameterState> params_;
    std::mutex mutex_;  // For parameter changes (not audio thread)
};
```

### 8.2. Sample-Accurate Automation

```cpp
// Parameters can be updated sample-accurately within a buffer:

void AudioPlugin::process(const ProcessContext& ctx) {
    // Receive parameter changes with sample offsets
    for (const auto& change : ctx.parameter_changes) {
        // change.sample_offset = sample within this buffer
        // change.value = new value
        //
        // The plugin should apply changes at exact sample positions
        // for sample-accurate automation
    }
    
    // Process audio with per-sample parameter interpolation
    for (uint32_t s = 0; s < ctx.frames; ++s) {
        // Check for parameter changes at this sample
        while (change_index < ctx.parameter_changes.size() &&
               ctx.parameter_changes[change_index].sample_offset == s) {
            apply_parameter(ctx.parameter_changes[change_index]);
            ++change_index;
        }
        outputs[0][s] = process_sample(inputs[0][s]);
    }
}
```

---

## 9. Plugin Scanning

### 9.1. Scan Architecture

```cpp
class PluginScanner {
public:
    PluginScanner();
    
    // Scan paths (configurable)
    void add_scan_path(const std::string& path);
    void remove_scan_path(const std::string& path);
    std::vector<std::string> scan_paths() const;
    
    // Default scan paths per platform:
    // Windows:
    //   C:\Program Files\Common Files\VST3\
    //   C:\Program Files\Common Files\VST2\
    //   C:\Program Files\Common Files\CLAP\
    //   %APPDATA%\ARIA\Plugins\
    //
    // macOS:
    //   /Library/Audio/Plug-Ins/Components/
    //   /Library/Audio/Plug-Ins/VST3/
    //   /Library/Audio/Plug-Ins/CLAP/
    //   ~/Library/Audio/Plug-Ins/
    //
    // Linux:
    //   /usr/lib/vst3/
    //   /usr/lib/clap/
    //   ~/.vst3/
    //   ~/.clap/
    
    // Perform scan
    void scan(ScanMode mode);
    void cancel_scan();
    
    // Scan cache
    void save_cache(const std::string& path);
    void load_cache(const std::string& path);
    
    // Verification
    bool verify_plugin(const std::string& path);
    
    // Blacklist check
    bool is_blacklisted(const std::string& path);

private:
    // Per-format scanners
    std::unique_ptr<FormatScanner> clap_scanner_;
    std::unique_ptr<FormatScanner> vst3_scanner_;
    std::unique_ptr<FormatScanner> au_scanner_;   // macOS only
    std::unique_ptr<FormatScanner> lv2_scanner_;   // Linux only
    
    // Thread pool for parallel scanning
    ThreadPool scan_threads_;
    
    // Scan cache (JSON)
    PluginCache cache_;
};
```

### 9.2. Incremental Scan

```cpp
// On startup:
// 1. Load cached plugin list from disk
// 2. Check file modification times
// 3. Only scan new/changed files
// 4. Remove plugins for deleted files
// 5. Save updated cache
//
// Full rescan (user-initiated):
// 1. Clear cache
// 2. Scan all paths
// 3. Verify each plugin (load, query, unload)
// 4. Save cache

struct PluginCache {
    uint64_t last_scan_time;
    std::unordered_map<std::string, PluginCacheEntry> entries;
    // Key: file path
};

struct PluginCacheEntry {
    std::string file_path;
    uint64_t file_size;
    uint64_t modified_time;
    std::string file_hash;        // Quick hash for change detection
    std::vector<PluginDescriptor> plugins;  // May contain multiple plugins per file
};
```

---

## 10. Preset System

### 10.1. Preset Format

```cpp
class PresetManager {
public:
    // Load/save preset
    bool load_preset(PluginID plugin_id, const std::string& path);
    bool save_preset(PluginID plugin_id, const std::string& path);
    
    // Browser
    std::vector<PresetInfo> browse_presets(PluginID plugin_id) const;
    std::vector<PresetInfo> search_presets(const std::string& query) const;
    
    // User presets
    std::string user_preset_path(PluginID plugin_id) const;
    bool save_user_preset(PluginID plugin_id, const std::string& name);
    bool delete_user_preset(PluginID plugin_id, const std::string& name);
    
    // Factory presets (shipped with ARIA)
    std::vector<PresetInfo> factory_presets(PluginID plugin_id) const;

private:
    // Preset storage paths
    std::string factory_path_;     // Read-only
    std::string user_path_;        // Read-write
    
    // Preset database (SQLite)
    PresetDatabase db_;
};

struct PresetInfo {
    std::string name;
    std::string path;
    std::string category;      // "Bass", "Lead", "Pad", "FX", etc.
    std::string author;
    std::string tags;           // Comma-separated
    bool is_factory;            // Factory vs user
    uint64_t file_size;
};
```

### 10.2. CLAP Preset Format

```cpp
// ARIA uses the CLAP preset format (.clap-preset) for all plugins
// This is an open format that any CLAP host can read:
//
// .clap-preset file structure:
// ┌─────────────────────────────┐
// │ Header                      │
// │ - Magic: "CLAPPRES"         │
// │ - Version: 1                │
// ├─────────────────────────────┤
// │ Metadata                    │
// │ - Plugin ID                 │
// │ - Name                      │
// │ - Author                    │
// │ - Tags                      │
// │ - Created                   │
// ├─────────────────────────────┤
// │ Plugin State (CLAP format)  │
// │ - Parameters                │
// │ - Custom data               │
// └─────────────────────────────┘

// Presets are portable across any CLAP-compatible DAW.
```

---

## 11. Plugin GUI

### 11.1. Editor Integration

```cpp
class PluginEditorHost {
public:
    // Create embedded editor window
    bool create_editor(AudioPlugin* plugin, void* parent_view);
    void destroy_editor();
    
    // Window management
    void set_size(uint32_t width, uint32_t height);
    void get_size(uint32_t& width, uint32_t& height) const;
    void set_scale(double scale_factor);  // HiDPI support
    
    // Event forwarding
    void on_mouse_event(const MouseEvent& event);
    void on_key_event(const KeyEvent& event);
    void on_timer();                      // Idle call for plugin
    
    // Returns whether plugin has its own editor
    bool plugin_has_editor() const;
    
    // Embedding modes
    enum class EmbedMode {
        Embedded,       // Directly in ARIA window
        Floating,       // Separate OS window
        Detached        // Fully independent window
    };
    void set_embed_mode(EmbedMode mode);

private:
    AudioPlugin* plugin_ = nullptr;
    EmbedMode mode_ = EmbedMode::Embedded;
    
    // Platform-specific view handles
    void* native_view_ = nullptr;     // The plugin's view
    void* container_view_ = nullptr;  // ARIA's wrapper
};
```

---

## 12. Plugin Delay Compensation

```cpp
class PDCManager {
public:
    // Recalculate all PDC
    void recalculate(const std::vector<AudioPlugin*>& chain);
    
    // Get delay for a specific plugin
    uint32_t plugin_latency(PluginID id) const;
    
    // Get total chain delay
    uint32_t chain_delay(const std::vector<AudioPlugin*>& chain) const;
    
    // Maximum delay across all chains (for track alignment)
    uint32_t max_delay() const;
    
    // Apply compensation delay
    void apply_compensation(AudioBlock& block, uint32_t delay_samples);

private:
    struct PDCState {
        std::unordered_map<PluginID, uint32_t> plugin_delays;
        uint32_t max_delay = 0;
    };
    PDCState state_;
    
    // Delay buffer for early chains
    struct DelayBuffer {
        std::vector<float> data;
        uint32_t write_pos = 0;
        uint32_t size = 0;
    };
    std::unordered_map<TrackID, DelayBuffer> delay_buffers_;
};
```

---

## 13. API Reference

### 13.1. Public API

```cpp
class PluginHostAPI {
public:
    // Scanning
    void scan_plugins(PluginFactory::ScanMode mode);
    bool is_scanning() const;
    double scan_progress() const;
    void cancel_scan();
    
    // Plugin discovery
    std::vector<PluginDescriptor> all_plugins() const;
    std::vector<PluginDescriptor> plugins_by_category(PluginCategory cat) const;
    std::vector<PluginDescriptor> search_plugins(const std::string& query) const;
    
    // Plugin instantiation
    std::unique_ptr<AudioPlugin> instantiate(const PluginID& id);
    std::unique_ptr<AudioPlugin> instantiate(const std::string& path);
    
    // Presets
    PresetManager& presets();
    
    // Blacklist
    PluginBlacklist& blacklist();
    
    // Plugin paths
    void add_scan_path(const std::string& path);
    std::vector<std::string> scan_paths() const;
    
    // State
    void save_state(const std::string& path);
    void load_state(const std::string& path);
};
```

---

## 14. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-PH-001 | Unified Plugin Interface | 🔲 Pending |
| RFC-PH-002 | CLAP Host Implementation | 🔲 Pending |
| RFC-PH-003 | CLAP Native Plugin Factory | 🔲 Pending |
| RFC-PH-004 | VST3 Host Implementation | 🔲 Pending |
| RFC-PH-005 | AU Host (macOS) | 🔲 Pending |
| RFC-PH-006 | LV2 Host (Linux) | 🔲 Pending |
| RFC-PH-007 | Plugin Sandboxing (Levels 0-3) | 🔲 Pending |
| RFC-PH-008 | Out-of-Process Sandbox | 🔲 Pending |
| RFC-PH-009 | Parameter System & Sample-Accurate Automation | 🔲 Pending |
| RFC-PH-010 | Plugin Scanning (Incremental) | 🔲 Pending |
| RFC-PH-011 | Plugin Cache & Verification | 🔲 Pending |
| RFC-PH-012 | CLAP Preset Format | 🔲 Pending |
| RFC-PH-013 | Preset Manager & Database | 🔲 Pending |
| RFC-PH-014 | Plugin Editor Host | 🔲 Pending |
| RFC-PH-015 | Plugin Delay Compensation | 🔲 Pending |
| RFC-PH-016 | Plugin Blacklist | 🔲 Pending |
| RFC-PH-017 | Plugin MIDI Interface | 🔲 Pending |
| RFC-PH-018 | Crash Recovery & Restart | 🔲 Pending |
| RFC-PH-019 | Plugin State Serialization | 🔲 Pending |

---

## Appendix A: Plugin Format Comparison

| Feature | CLAP | VST3 | AU | LV2 |
|---|---|---|---|---|
| Open standard | ✅ Yes | ❌ Proprietary | ❌ Apple only | ✅ Yes |
| Cross-platform | ✅ | ✅ | ❌ macOS only | ✅ |
| Sandboxing support | ✅ Native | ⚠️ Partial | ❌ No | ⚠️ Partial |
| Per-note expression | ✅ MPE | ⚠️ VST3 MPE | ✅ MPE | ⚠️ Limited |
| Sample-accurate automation | ✅ Native | ✅ | ✅ | ✅ |
| Parameter modulation | ✅ Native | ✅ | ✅ | ⚠️ |
| Thread safety | ✅ Designed | ⚠️ Legacy | ✅ | ⚠️ |
| License | MIT | Proprietary | Proprietary | ISC |
| ARIA priority | **Native (P0)** | P0 (compat) | P0 (macOS) | P1 |

## Appendix B: Default Scan Paths

```
Windows:
  CLAP: %PROGRAMFILES%\Common Files\CLAP\
  VST3: %PROGRAMFILES%\Common Files\VST3\
  User: %APPDATA%\ARIA\Plugins\

macOS:
  CLAP: /Library/Audio/Plug-Ins/CLAP/
  VST3: /Library/Audio/Plug-Ins/VST3/
  AU:   /Library/Audio/Plug-Ins/Components/
  User: ~/Library/Audio/Plug-Ins/

Linux:
  CLAP: /usr/lib/clap/
  VST3: /usr/lib/vst3/
  LV2:  /usr/lib/lv2/
  User: ~/.clap/
        ~/.vst3/
        ~/.lv2/
```
