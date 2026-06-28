#ifndef CLAP_CLAP_H
#define CLAP_CLAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Version ──────────────────────────────────────────────────────────

typedef struct clap_version {
    uint32_t major;
    uint32_t minor;
    uint32_t revision;
} clap_version;

#define CLAP_VERSION_INIT                                                                    \
    {                                                                                        \
        1, 2, 0                                                                              \
    }

static const clap_version CLAP_VERSION = CLAP_VERSION_INIT;

// ── Identifiers ──────────────────────────────────────────────────────

typedef uint32_t clap_id;

// ── Audio Buffer ─────────────────────────────────────────────────────

typedef struct clap_audio_buffer {
    float **data32;
    double **data64;
    uint32_t channel_count;
    uint32_t latency;
} clap_audio_buffer;

// ── Events ───────────────────────────────────────────────────────────

typedef uint32_t clap_event_time;

typedef struct clap_event_header {
    clap_event_time time;
    uint16_t size;
    uint16_t type;
    uint32_t flags;
} clap_event_header;

typedef struct clap_event_param_value {
    clap_event_header header;
    clap_id param_id;
    double value;
    double modulation;
} clap_event_param_value;

typedef struct clap_event_param_mod {
    clap_event_header header;
    clap_id param_id;
    double amount;
    double modulation;
} clap_event_param_mod;

typedef struct clap_event_note {
    clap_event_header header;
    clap_id note_id;
    int16_t port_index;
    int16_t channel;
    int16_t key;
    int16_t velocity;
    double pitch;
    double tune;
} clap_event_note;

typedef struct clap_event_note_expression {
    clap_event_header header;
    clap_id note_id;
    int16_t port_index;
    int16_t channel;
    int16_t key;
    int32_t expression_id;
    double value;
} clap_event_note_expression;

typedef struct clap_event_midi {
    clap_event_header header;
    uint8_t port_index;
    uint8_t data[3];
} clap_event_midi;

typedef struct clap_event_midi_sysex {
    clap_event_header header;
    uint8_t port_index;
    uint8_t *buffer;
    uint32_t size;
} clap_event_midi_sysex;

// ── Events Array ─────────────────────────────────────────────────────

typedef struct clap_input_events {
    void *ctx;
    uint32_t (*size)(const struct clap_input_events *list);
    const clap_event_header *(*get)(const struct clap_input_events *list, uint32_t index);
    bool (*try_push)(const struct clap_input_events *list, const clap_event_header *event);
} clap_input_events;

typedef struct clap_output_events {
    void *ctx;
    uint32_t (*size)(const struct clap_output_events *list);
    const clap_event_header *(*get)(const struct clap_output_events *list, uint32_t index);
    bool (*try_push)(const struct clap_output_events *list, const clap_event_header *event);
} clap_output_events;

// ── Process ──────────────────────────────────────────────────────────

typedef struct clap_process {
    clap_audio_buffer *audio_input;
    clap_audio_buffer *audio_output;
    const clap_input_events *in_events;
    clap_output_events *out_events;
    uint32_t frames_count;
    uint64_t steady_time;
    uint64_t transport;
} clap_process;

typedef struct clap_process_transport {
    uint32_t flags;
    double song_pos_beats;
    double song_pos_seconds;
    int64_t song_pos_frames;
    double tempo;
    double tempo_ratio;
    int32_t time_sig_num;
    int32_t time_sig_denom;
} clap_process_transport;

// ── Plugin Descriptor ────────────────────────────────────────────────

typedef struct clap_plugin_descriptor {
    const char *clap_version;
    const char *id;
    const char *name;
    const char *vendor;
    const char *url;
    const char *manual_url;
    const char *support_url;
    const char *version;
    const char *description;
    const char *features;
} clap_plugin_descriptor;

// ── Host / Plugin structs (forward decl) ─────────────────────────────

struct clap_host;
struct clap_plugin;

// ── Plugin ───────────────────────────────────────────────────────────

typedef struct clap_plugin {
    const clap_plugin_descriptor *desc;
    void *plugin_data;
    bool (*init)(const struct clap_plugin *plugin);
    void (*destroy)(const struct clap_plugin *plugin);
    bool (*activate)(const struct clap_plugin *plugin,
                     double sample_rate,
                     uint32_t min_frames,
                     uint32_t max_frames);
    void (*deactivate)(const struct clap_plugin *plugin);
    bool (*start_processing)(const struct clap_plugin *plugin);
    void (*stop_processing)(const struct clap_plugin *plugin);
    void (*process)(const struct clap_plugin *plugin, const clap_process *process);
    void (*reset)(const struct clap_plugin *plugin);
} clap_plugin;

// ── Host ─────────────────────────────────────────────────────────────

typedef struct clap_host_descriptor {
    const char *clap_version;  // Version string, e.g. "1.2.0"
    const char *name;
    const char *vendor;
    const char *url;
    const char *version;       // Host version
} clap_host_descriptor;

typedef struct clap_host {
    const clap_host_descriptor *desc;
    void *host_data;
    bool (*get_extension)(const struct clap_host *host, const char *extension_id, const void **extension);
    void (*request_restart)(const struct clap_host *host);
    void (*request_process)(const struct clap_host *host);
    void (*request_callback)(const struct clap_host *host);
} clap_host;

// ── Plugin Entry ─────────────────────────────────────────────────────

typedef struct clap_plugin_entry {
    clap_version version;
    bool (*init)(const char *plugin_path);
    void (*deinit)(void);
    uint32_t (*get_plugin_count)(void);
    const clap_plugin_descriptor *(*get_plugin_descriptor)(uint32_t index);
    const clap_plugin *(*create_plugin)(const struct clap_host *host, const char *plugin_id);
} clap_plugin_entry;

// ── Extension IDs ────────────────────────────────────────────────────

#define CLAP_EXT_AUDIO_PORTS     "clap.audio-ports"
#define CLAP_EXT_AUDIO_PORTS_CONFIG "clap.audio-ports-config"
#define CLAP_EXT_NOTE_PORTS      "clap.note-ports"
#define CLAP_EXT_PARAMS          "clap.params"
#define CLAP_EXT_LATENCY         "clap.latency"
#define CLAP_EXT_TAIL            "clap.tail"
#define CLAP_EXT_STATE           "clap.state"
#define CLAP_EXT_GUI             "clap.gui"
#define CLAP_EXT_MPE             "clap.mpe"

// ── Audio Ports ──────────────────────────────────────────────────────

typedef struct clap_audio_ports_info {
    clap_id id;
    const char *name;
    uint32_t flags;
    uint32_t channel_count;
    uint32_t port_type;
    uint32_t in_place_pair;
} clap_audio_port_info;

typedef struct clap_plugin_audio_ports {
    uint32_t (*count)(const struct clap_plugin *plugin, bool is_input);
    bool (*get)(const struct clap_plugin *plugin,
                uint32_t index,
                bool is_input,
                clap_audio_port_info *info);
} clap_plugin_audio_ports;

typedef struct clap_host_audio_ports {
    bool (*is_reserved)(const struct clap_host *host, clap_id port_id);
} clap_host_audio_ports;

// ── Note Ports ───────────────────────────────────────────────────────

typedef struct clap_note_port_info {
    clap_id id;
    const char *name;
    uint32_t supported_dialects;
    uint32_t preferred_dialect;
} clap_note_port_info;

typedef struct clap_plugin_note_ports {
    uint32_t (*count)(const struct clap_plugin *plugin, bool is_input);
    bool (*get)(const struct clap_plugin *plugin,
                uint32_t index,
                bool is_input,
                clap_note_port_info *info);
} clap_plugin_note_ports;

// ── Params ───────────────────────────────────────────────────────────

typedef struct clap_param_info {
    clap_id id;
    const char *name;
    const char *module;
    double min_value;
    double max_value;
    double default_value;
    uint32_t flags;
    uint32_t cookie;
} clap_param_info;

typedef struct clap_plugin_params {
    bool (*count)(const struct clap_plugin *plugin, uint32_t *param_count);
    bool (*get_info)(const struct clap_plugin *plugin,
                     uint32_t param_index,
                     clap_param_info *param_info);
    bool (*get_value)(const struct clap_plugin *plugin, clap_id param_id, double *value);
    bool (*value_to_text)(const struct clap_plugin *plugin,
                          clap_id param_id,
                          double value,
                          char *out_buf,
                          uint32_t out_buf_size);
    bool (*text_to_value)(const struct clap_plugin *plugin,
                          clap_id param_id,
                          const char *text,
                          double *out_value);
    void (*flush)(const struct clap_plugin *plugin,
                  const clap_input_events *in,
                  const clap_output_events *out);
} clap_plugin_params;

typedef struct clap_host_params {
    void (*rescan)(const struct clap_host *host, uint32_t flags);
    void (*request_flush)(const struct clap_host *host);
} clap_host_params;

// ── Latency ──────────────────────────────────────────────────────────

typedef struct clap_plugin_latency {
    uint32_t (*get)(const struct clap_plugin *plugin);
} clap_plugin_latency;

// ── Tail ─────────────────────────────────────────────────────────────

typedef struct clap_plugin_tail {
    uint32_t (*get)(const struct clap_plugin *plugin);
} clap_plugin_tail;

// ── State ────────────────────────────────────────────────────────────

typedef struct clap_ostream {
    void *ctx;
    bool (*write)(const struct clap_ostream *stream, const void *buffer, uint64_t size);
} clap_ostream;

typedef struct clap_istream {
    void *ctx;
    bool (*read)(const struct clap_istream *stream, void *buffer, uint64_t size);
} clap_istream;

typedef struct clap_plugin_state {
    bool (*save)(const struct clap_plugin *plugin, const clap_ostream *stream);
    bool (*load)(const struct clap_plugin *plugin, const clap_istream *stream);
} clap_plugin_state;

// ── MPE ──────────────────────────────────────────────────────────────

typedef struct clap_host_mpe {
    void (*changed)(const struct clap_host *host);
} clap_host_mpe;

// ── GUI ──────────────────────────────────────────────────────────────

typedef struct clap_plugin_gui {
    bool (*is_api_supported)(const struct clap_plugin *plugin, const char *api, bool is_floating);
    bool (*get_preferred_api)(const struct clap_plugin *plugin, const char **api, bool *is_floating);
    bool (*create)(const struct clap_plugin *plugin, const char *api, bool is_floating);
    void (*destroy)(const struct clap_plugin *plugin);
    bool (*set_scale)(const struct clap_plugin *plugin, double scale);
    bool (*get_size)(const struct clap_plugin *plugin, uint32_t *width, uint32_t *height);
    bool (*can_resize)(const struct clap_plugin *plugin);
    bool (*get_resize_hints)(const struct clap_plugin *plugin, uint32_t *hints);
    bool (*adjust_size)(const struct clap_plugin *plugin, uint32_t *width, uint32_t *height);
    bool (*set_size)(const struct clap_plugin *plugin, uint32_t width, uint32_t height);
    bool (*set_parent)(const struct clap_plugin *plugin, const void *window);
    bool (*set_transient)(const struct clap_plugin *plugin, const void *window);
    void (*suggest_title)(const struct clap_plugin *plugin, const char *title);
} clap_plugin_gui;

typedef struct clap_host_gui {
    void (*resize_hints_changed)(const struct clap_host *host);
    bool (*request_resize)(const struct clap_host *host, uint32_t width, uint32_t height);
    void (*request_show)(const struct clap_host *host);
    void (*request_hide)(const struct clap_host *host);
    void (*closed)(const struct clap_host *host, bool was_destroyed);
} clap_host_gui;

#ifdef __cplusplus
}
#endif

#endif /* CLAP_CLAP_H */