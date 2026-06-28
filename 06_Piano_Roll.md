# ARIA DAW — Piano Roll Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Language**: C++20/23 (graphics engine) + TypeScript (UI layer)
> **GPU**: Fully GPU-accelerated via WebGPU + Skia

---

## Table of Contents

1. [Overview](#1-overview)
2. [Data Model](#2-data-model)
3. [Rendering Architecture](#3-rendering-architecture)
4. [Grid & Navigation](#4-grid--navigation)
5. [Note Rendering](#5-note-rendering)
6. [Tools & Interactions](#6-tools--interactions)
7. [Selection System](#7-selection-system)
8. [Velocity Editing](#8-velocity-editing)
9. [Expression Lanes](#9-expression-lanes)
10. [Automation Integration](#10-automation-integration)
11. [Ghost Notes](#11-ghost-notes)
12. [Scale & Chord Tools](#12-scale--chord-tools)
13. [Quantization & Humanization](#13-quantization--humanization)
14. [Keyboard & Mouse](#14-keyboard--mouse)
15. [Performance](#15-performance)
16. [Multi-Monitor & Layout](#16-multi-monitor--layout)
17. [API Reference](#17-api-reference)
18. [RFC Index](#18-rfc-index)

---

## 1. Overview

### 1.1. Philosophy

The Piano Roll is ARIA's flagship editor. It is not a grid with rectangles — it is a **vector illustration canvas** optimized for MIDI editing. Every note is a rich object with position, duration, velocity, color, channel, expression data, MPE, and embedded automation.

It combines the best of FL Studio's piano roll (tools, workflow) with Ableton's expression editing (automation within the roll) and adds capabilities unique to ARIA.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Max notes | 100,000+ with smooth 60 FPS scrolling |
| Input latency | < 1ms from key press to visual feedback |
| Snap accuracy | Sub-pixel at all zoom levels |
| Tool switching | Instant (no perceptible delay) |
| Expression lanes | Unlimited concurrent lanes |
| Grid resolution | 1/1024th of a beat (sub-PPQN) |

### 1.3. Rendering Philosophy

```
Everything is GPU-rendered:
  ✓ Note rectangles (as GPU polygon meshes)
  ✓ Velocity bars (as GPU vertex buffers)
  ✓ Expression curves (as GPU line strips with tessellation)
  ✓ Grid lines (as GPU line batches)
  ✓ Ghost notes (with alpha blending)
  ✓ Keyboard (as textured quads)
  
Nothing is CPU-rasterized:
  ✗ No GDI painting
  ✗ No bitmap blitting
  ✗ No software rendering fallback
```

---

## 2. Data Model

### 2.1. Note Object

Every note in ARIA's piano roll is a first-class object with rich properties:

```cpp
struct Note {
    // Core properties
    NoteID id;                  // Unique persistent ID
    uint64_t start_ppqn;        // Start position (PPQN)
    uint64_t duration_ppqn;     // Duration (PPQN)
    uint8_t key;                // MIDI note number 0-127
    uint8_t velocity;           // 0-127
    uint8_t release_velocity;   // 0-127
    uint8_t channel;            // MIDI channel 0-15
    uint32_t track_id;          // Parent track

    // Visual properties
    Color color;                // Per-note color (inherits from track if default)
    float opacity;              // 0.0 - 1.0
    bool muted;                 // Muted (ghosted rendering)
    bool locked;                // Locked (not editable)

    // Expression data (per-note)
    MPEExpression mpe;          // MPE bend/timbre/pressure
    std::vector<NoteExpressionEvent> expressions;  // Per-note CC

    // Metadata
    std::string label;          // Optional note label (e.g., "C#5")
    uint64_t flags;             // Bitfield (selected, ghost, error, etc.)

    // Cached render data
    float cached_x, cached_y;           // Screen position (invalidated on change)
    float cached_width, cached_height;  // Screen size
};
```

### 2.2. Note Collection

```cpp
class NoteCollection {
public:
    using iterator = std::vector<Note>::iterator;
    using const_iterator = std::vector<Note>::const_iterator;

    // CRUD
    NoteID add_note(const Note& note);
    void remove_note(NoteID id);
    void update_note(NoteID id, const Note& note);
    void clear();
    void reserve(size_t capacity);

    // Queries
    Note* find(NoteID id);
    const Note* find(NoteID id) const;
    std::vector<Note*> find_in_range(uint64_t start_ppqn, uint64_t end_ppqn,
                                     uint8_t key_low = 0, uint8_t key_high = 127);
    std::vector<Note*> find_by_key(uint8_t key, uint8_t channel = 0);

    // Spatial index (for fast lookup during rendering)
    void rebuild_spatial_index();
    std::vector<Note*> query_rect(const Rect& viewport_ppqn);

    // Selection
    void select(NoteID id);
    void deselect(NoteID id);
    void select_all();
    void deselect_all();
    void toggle_selection(NoteID id);
    const std::unordered_set<NoteID>& selected() const;

    // Bulk operations
    void transpose(int8_t semitones);
    void shift_horizontal(int64_t ppqn_amount);
    void multiply_velocity(double factor);
    void apply_curve(uint8_t start_vel, uint8_t end_vel, CurveType curve);

    // Sorting
    void sort_by_start_time();
    void sort_by_key();

    // Capacity
    size_t size() const;
    bool empty() const;
    size_t memory_usage() const;

private:
    std::vector<Note> notes_;
    std::unordered_map<NoteID, size_t> id_index_;
    std::unique_ptr<SpatialIndex> spatial_;  // R-tree or grid for spatial queries

    // Selection
    std::unordered_set<NoteID> selected_notes_;
};
```

### 2.3. Note ID System

```cpp
// Note IDs are globally unique within a project.
// Format: timestamp + counter (64-bit)
//  - 40 bits: Unix timestamp (seconds)
//  - 24 bits: counter (wraps within same second)
// This gives unique IDs even with concurrent edits.

struct NoteID {
    uint64_t value;

    bool operator==(const NoteID& other) const { return value == other.value; }
    bool operator!=(const NoteID& other) const { return value != other.value; }
};
```

### 2.4. Expression Data Model

```cpp
struct NoteExpressionEvent {
    uint64_t offset_ppqn;      // Offset from note start (PPQN)
    uint8_t type;              // CC, PitchBend, ChannelPressure, PolyPressure
    uint8_t cc_number;         // If type == CC
    uint16_t value;            // 14-bit resolution (0-16383)
    Interpolation interpolation; // Step, Linear, Bezier, Smooth
};

struct MPEExpression {
    // MPE per-note data (pitch bend range is configured per-device)
    int32_t pitch_bend;        // -8192 to 8191 (14-bit, ±2 semitones default)
    uint8_t timbre;            // CC 74 (0-127)
    uint8_t pressure;          // Channel pressure (0-127)

    // Utility
    bool is_active() const;
    void apply_from_mpe_channel(const uint8_t* mpe_data);
};
```

---

## 3. Rendering Architecture

### 3.1. Render Pipeline

```
NoteCollection (CPU)
    │
    ▼
Spatial Query → Viewport-visible notes
    │
    ▼
Culling → Only visible + margin notes
    │
    ▼
Vertex Generation → For each visible note:
    │   ├── Position (x, y) from start_ppqn + key
    │   ├── Size (w, h) from duration_ppqn + key_height
    │   ├── Color from note.color * velocity alpha
    │   ├── Rounded corners (4 vertex quads with bevel)
    │   └── Shadow (separate pass with blur)
    │
    ├──► GPU Vertex Buffer Upload (persistent mapping)
    │
    ├──► Draw Pass 1: Note shadows (gaussian blur)
    ├──► Draw Pass 2: Note bodies (instanced quads)
    ├──► Draw Pass 3: Note overlays (velocity bar, mute X)
    ├──► Draw Pass 4: Selection outlines
    ├──► Draw Pass 5: Ghost notes (alpha blend)
    └──► Draw Pass 6: Grid + ruler overlays
```

### 3.2. Render Layers (Z-Order)

| Layer | Content | Blend Mode |
|---|---|---|
| 0 | Background grid | Solid |
| 1 | Ghost notes (other clips) | Alpha 0.3 |
| 2 | Automation curves (background) | Alpha 0.5 |
| 3 | Note shadows | Gaussian blur |
| 4 | Note bodies | Solid |
| 5 | Note velocity bars | Solid |
| 6 | Note selection overlay | Alpha 0.3 |
| 7 | Selection rectangle (marquee) | Alpha 0.2 + border |
| 8 | Playhead / loop markers | Solid |
| 9 | Cursor / tool indicators | Solid |
| 10 | Expression lane curves | Solid (over notes) |

### 3.3. GPU Resources

```cpp
class PianoRollRenderer {
public:
    void init(GPUDevice* device);
    void render(const RenderState& state);
    void resize(uint32_t width, uint32_t height);
    void invalidate_notes();
    void set_viewport(const Rect& ppqn_bounds, const Rect& pixel_bounds);

private:
    // Persistent GPU resources
    GPUBuffer* vertex_buffer_;      // Dynamic vertex buffer (persistent mapped)
    GPUBuffer* index_buffer_;       // Index buffer (static, pre-allocated)
    GPUBuffer* instance_buffer_;    // Per-note instance data

    // Shaders
    GPUShader* note_shader_;        // Note body vertex/fragment
    GPUShader* grid_shader_;        // Grid lines
    GPUShader* blur_shader_;        // Gaussian blur (shadows)

    // Render passes
    RenderPass* note_pass_;
    RenderPass* shadow_pass_;
    RenderPass* overlay_pass_;

    // State tracking
    uint32_t last_note_count_ = 0;
    bool needs_vertex_update_ = true;
    ViewTransform view_transform_;
};
```

### 3.4. Viewport Culling

```cpp
struct ViewTransform {
    // Project → Screen
    double ppqn_per_pixel_x;
    double pixels_per_semitone_y;
    double scroll_x;          // Scroll offset (PPQN)
    double scroll_y;          // Scroll offset (semitones)

    // Note dimensions
    double note_min_height_px;   // Min 4px even when zoomed far out
    double note_min_width_px;    // Min 2px

    // Conversion
    double ppqn_to_screen_x(uint64_t ppqn) const;
    double semitone_to_screen_y(uint8_t key) const;
    uint64_t screen_x_to_ppqn(double x) const;
    uint8_t screen_y_to_semitone(double y) const;
    Rect viewport_ppqn() const;       // Visible range in PPQN
    Rect viewport_screen() const;      // Visible range in pixels
};

// Culling: only process notes within viewport + margin
const uint64_t MARGIN_PPQN = 960 * 4;  // 1 bar margin
```

---

## 4. Grid & Navigation

### 4.1. Grid Levels

| Zoom Level | Grid Resolution | PPQN per cell | Labels |
|---|---|---|---|
| 1 (Bar) | Bars | 3840 | 1, 2, 3, 4... |
| 2 (Beat) | Beats | 960 | 1.1, 1.2, 1.3, 1.4... |
| 3 (Eighth) | Eighth notes | 480 | 1.1.1, 1.1.2... |
| 4 (Sixteenth) | Sixteenth notes | 240 | Grid ticks |
| 5 (32nd) | 32nd notes | 120 | Grid ticks |
| 6 (64th) | 64th notes | 60 | Grid ticks |
| 7 (128th) | 128th notes | 30 | Grid ticks |
| 8 (1/256) | Max resolution | 15 | No labels |

### 4.2. Snap System

```cpp
class SnapSystem {
public:
    enum class SnapMode {
        None,           // Free placement
        Grid,           // Snap to grid
        Relative,       // Snap relative to existing notes
        Magnetic,       // Attract to nearby grid lines (magnetic)
        Adaptive         // Auto-adjust grid based on zoom
    };

    // Snap a position to the nearest grid point
    uint64_t snap_ppqn(uint64_t position, SnapMode mode);

    // Snap with strength (0.0 = no snap, 1.0 = hard snap)
    uint64_t snap_ppqn(uint64_t position, double strength);

    // Current grid resolution
    uint32_t grid_resolution_ppqn() const;

    // Toggle snap modes
    void set_snap_mode(SnapMode mode);
    void toggle_snap();
    void cycle_snap_resolution();

    // Visual grid lines
    std::vector<GridLine> get_grid_lines(const ViewTransform& view,
                                          const Rect& viewport);

private:
    SnapMode mode_ = SnapMode::Grid;
    uint32_t resolution_ = 240;  // 16th notes default
    bool snap_enabled_ = true;
};
```

### 4.3. Navigation Controls

| Action | Keyboard | Mouse |
|---|---|---|
| Scroll horizontal | Shift + Scroll | Horizontal scroll |
| Scroll vertical | Scroll | Vertical scroll |
| Zoom horizontal | Ctrl + Scroll | Alt + Scroll |
| Zoom vertical | Ctrl + Shift + Scroll | Ctrl + Alt + Scroll |
| Pan | Space + Drag | Middle mouse drag |
| Go to start | Home | — |
| Go to end | End | — |
| Jump to note | N (then type note) | — |
| Zoom to selection | Z | Double-click on note |
| Fit all | Shift + Z | — |

---

## 5. Note Rendering

### 5.1. Note Visual Properties

```
Note at default velocity (100):
┌─────────────────┐
│ ████████████████ │  ← Note body (colored by track/channel)
│ ████            │  ← Velocity layer (opacity = velocity/127)
│ ████            │
│ ████            │
└─────────────────┘

Note at velocity 127 (full):
┌─────────────────┐
│ ████████████████ │
│ ████████████████ │
│ ████████████████ │
│ ████████████████ │
└─────────────────┘

Muted note:
┌─────────────────┐
│ ░░░░░░░░░░░░░░░░ │  ← Cross-hatch pattern or reduced opacity
│ ░░░░░░░░░░░░░░░░ │
│ ░░░░░░░░░░░░░░░░ │
│ ░░░░░░░░░░░░░░░░ │
└─────────────────┘

Selected note:
┌─────────────────┐
│ ┌─────────────┐ │  ← White outline (2px)
│ │ ████████████ │ │
│ │ ████████████ │ │
│ └─────────────┘ │
└─────────────────┘
```

### 5.2. Note Corner Radius

Note corners scale with zoom level and duration:

```cpp
// Short notes (< 1/16 beat) → sharp corners (0px radius)
// Medium notes (1/4 beat) → subtle radius (2px)
// Long notes (> 1 bar) → pronounced radius (4px max)

float calculate_corner_radius(const Note& note, const ViewTransform& view) {
    float width_px = view.ppqn_to_width_px(note.duration_ppqn);
    if (width_px < 8.0f) return 0.0f;  // Too small for radius
    if (width_px < 20.0f) return 1.0f;
    if (width_px < 50.0f) return 2.0f;
    return std::min(4.0f, width_px * 0.08f);  // Scale with size, cap at 4px
}
```

### 5.3. Note Color System

```cpp
struct NoteColor {
    Color body;             // Main note color
    Color velocity;         // Velocity layer (overlaid on body)
    Color outline;          // Selection border
    Color shadow;           // Drop shadow
    Color muted_overlay;    // Muted texture pattern
    float opacity;          // 0.0 - 1.0

    static NoteColor from_note(const Note& note, const Theme& theme);
};
```

**Color precedence:**
1. Per-note color (if explicitly set)
2. Track color (inherited)
3. MIDI channel color (configurable)
4. Velocity gradient (blue → green → yellow → red)
5. Scale degree color (tonic, dominant, etc.)

---

## 6. Tools & Interactions

### 6.1. Tool System

```cpp
enum class Tool {
    Pencil,         // Draw/create notes
    Select,         // Select and manipulate
    Paint,          // Paint notes (continuous creation)
    Erase,          // Delete notes on click/drag
    Cut,            // Split notes at position
    Glue,           // Merge adjacent notes
    Ramp,           // Velocity ramp (drag across notes)
    Mute,           // Toggle note mute
    Zoom,           // Zoom to rectangle
    Measure         // Measure intervals
};

class ToolManager {
public:
    void set_active_tool(Tool tool);
    Tool active_tool() const;

    // Input handlers
    void on_mouse_down(const MouseEvent& event);
    void on_mouse_move(const MouseEvent& event);
    void on_mouse_up(const MouseEvent& event);
    void on_key_down(const KeyEvent& event);

    // Tool-specific cursors
    Cursor cursor_for_tool(Tool tool) const;

private:
    Tool active_tool_ = Tool::Select;
    std::unique_ptr<ToolHandler> handlers_[10];

    // Current operation state
    enum class OpState { Idle, Dragging, Selecting, Painting, Erasing };
    OpState state_ = OpState::Idle;
};
```

### 6.2. Pencil Tool

```cpp
class PencilTool : public ToolHandler {
public:
    // Click: create note at cursor position
    // Drag from empty space: create note with custom duration
    // Drag from existing note: move note
    // Double-click: enter note label
    // Alt+Click on existing note: duplicate

    void on_mouse_down(const MouseEvent& event) override {
        auto hit = collection_.find_at(event.position);
        if (hit) {
            // Start drag (move/resize)
            drag_origin_ = hit->start_ppqn;
            drag_note_ = hit->id;
            state_ = OpState::Dragging;
        } else {
            // Create new note
            auto note = Note::create(event.position, snapped_position,
                                     default_duration_, active_channel_);
            auto id = collection_.add_note(note);
            drag_note_ = id;
            state_ = OpState::Dragging;
        }
    }

    void on_mouse_drag(const MouseEvent& event) override {
        // Update note position and duration
        Note* note = collection_.find(drag_note_);
        if (note) {
            note->start_ppqn = snap(event.position.ppqn);
            note->duration_ppqn = std::max(MIN_DURATION_PPQN, snap(event.duration_ppqn));
        }
    }

private:
    NoteID drag_note_;
    uint64_t drag_origin_;
    static constexpr uint64_t MIN_DURATION_PPQN = 60;  // 1/16th note minimum
};
```

### 6.3. Paint Tool

```cpp
class PaintTool : public ToolHandler {
public:
    // Click: create note at position
    // Drag: create continuous stream of notes at grid positions
    // Speed determines note density (faster = shorter notes)
    // Pressure (pen tablet) determines velocity

    void on_mouse_drag(const MouseEvent& event) override {
        // Create notes at each grid position the cursor passes
        uint64_t current_grid = snap(event.position.ppqn);
        if (current_grid != last_grid_position_) {
            auto note = Note::create(current_grid, current_grid + default_duration_,
                                     event.key, event.pressure * 127);
            collection_.add_note(note);
            last_grid_position_ = current_grid;
        }
    }

private:
    uint64_t last_grid_position_ = 0;
    uint64_t default_duration_ = 240;      // 16th note
};
```

### 6.4. Tool Behaviors

| Tool | Click | Drag | Double-Click | Alt+Click |
|---|---|---|---|---|
| **Pencil** | Create note | Create/resize note | Edit label | Duplicate note |
| **Select** | Select note | Marquee select | Select all same pitch | Deselect |
| **Paint** | Create note | Continuous paint | — | — |
| **Erase** | Delete note | Erase stroke | Delete all in bar | — |
| **Cut** | Split note at cursor | Split range | — | — |
| **Glue** | Merge overlapping | Select area to merge | — | — |
| **Ramp** | Set velocity point | Ramp across notes | Reset to default | — |
| **Mute** | Toggle mute | Mute range | — | — |

---

## 7. Selection System

### 7.1. Selection Types

```cpp
enum class SelectionType {
    None,
    Single,              // One note selected
    Multiple,            // Multiple notes (marquee or shift-click)
    Range,               // Time range selection (no notes)
    All                  // All notes selected
};

struct SelectionState {
    SelectionType type = SelectionType::None;
    std::unordered_set<NoteID> notes;

    // Range selection (for range operations)
    uint64_t range_start_ppqn;
    uint64_t range_end_ppqn;
    uint8_t range_key_low;
    uint8_t range_key_high;

    // Current drag operation
    enum class DragMode { None, Move, ResizeStart, ResizeEnd, Stretch };
    DragMode drag_mode = DragMode::None;
    uint64_t drag_anchor_ppqn;     // Snap reference point
    uint8_t drag_anchor_key;
};
```

### 7.2. Selection Visuals

```
Selected notes:
    ┌───── white outline (2px) ─────┐
    │  ┌─────────────────────────┐   │
    │  │ ██████████████████████  │   │
    │  │ ██████████████████████  │   │
    │  └─────────────────────────┘   │
    └──── resize handle (4px) ──────┘

    Note: selection is additive
    Shift+Click → add/remove from selection
    Marquee → select all within rectangle
    Ctrl+A → select all
```

### 7.3. Selection Handles

```cpp
// Each selected note shows resize handles:
// Left handle: move start (alters position, preserves end)
// Right handle: move end (alters duration)
// Top handle: transpose up
// Bottom handle: transpose down
// Corner handles: stretch (scale position + duration)

enum class Handle { None, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };

// Hit test for handles (only shown when note is selected and zoom > threshold)
Handle hit_test_handle(const Note& note, const Vec2& mouse_pos, const ViewTransform& view);
```

---

## 8. Velocity Editing

### 8.1. Velocity Display

```
Velocity lane (below the grid):
┌────────────────────────────────────┐
│ Note: C5  Velocity: 112/127       │  ← Header with selected note info
│                                    │
│  ┌─┐ ┌─┐   ┌─┐     ┌─┐ ┌─┐     │
│  │ │ │ │ ┌─┐ │ ┌─┐ │ │ │ │ ┌─┐  │  ← Velocity bars
│  │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │  │    (height = velocity value)
│  │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │  │
│  └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘  │
│   C5  D5  E5  F5  G5  A5  B5  C6  │  ← Note labels
└────────────────────────────────────┘

Each bar:
  - Width = note duration
  - Height = velocity / 127 * lane_height
  - Color: gradient from blue (low) to red (high)
  - Hover shows exact value tooltip
  - Selected bars show white outline
```

### 8.2. Velocity Editing Tools

| Input | Action |
|---|---|
| Click on velocity bar | Set exact velocity |
| Drag up/down on bar | Adjust velocity |
| Drag across multiple bars | Ramp velocity |
| Double-click | Reset to default (100) |
| Right-click + drag | Draw velocity curve |
| Alt + drag | Scale velocity proportionally |
| Ctrl + drag | Randomize within range |

### 8.3. Velocity Ramp

```cpp
// Ramp tool: creates a velocity gradient across selected notes
class VelocityRamp {
public:
    enum class CurveType {
        Linear,
        Exponential,
        Logarithmic,
        SCurve,
        InvertedSCurve,
        Custom
    };

    // Apply ramp from first to last selected note
    void apply_ramp(const std::vector<Note*>& notes,
                    uint8_t start_velocity,
                    uint8_t end_velocity,
                    CurveType curve);
};
```

### 8.4. Velocity Operations

| Operation | Description |
|---|---|
| Scale | Multiply all velocities by percentage (preserves ratios) |
| Add | Add/subtract constant to all velocities (clamped) |
| Compress | Reduce dynamic range (narrow min-max spread) |
| Expand | Increase dynamic range (widen min-max spread) |
| Randomize | Randomize within specified range |
| Reverse | Reverse velocity order of selection |
| Trending | Create trend curve through selection |
| Match | Match velocities to nearest grid position (Drum mode) |

---

## 9. Expression Lanes

### 9.1. Expression Lane System

Expression lanes appear below the note grid and show per-note or per-track expression data:

```
┌───────────────────────────────────────────┐
│  Piano Roll  │ Track: Synth 1  │ ⚙ Tools │  ← Header
├───────────────────────────────────────────┤
│  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐  │
│  │  │  │  │  │  │  │  │  │  │  │  │  │  │  ← Note grid
│  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘  │
├───────────────────────────────────────────┤
│  Pitch Bend  │ [─ ─ ─ ─ ─ ─ ─ ─ ─ ─]  │  │  ← Expression lane 1
│              │  \   / \   /              │  │
│              │   \ /   \ /               │  │
├───────────────────────────────────────────┤
│  CC 74 (Timbre) │ [─ ─ ─ ─ ─ ─ ─ ─ ─] │  │  ← Expression lane 2
│                 │  ┌──┐    ┌──┐         │  │
│                 │  │  │    │  │         │  │
├───────────────────────────────────────────┤
│  Velocity  │ [─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─] │  │  ← Velocity lane (always visible)
│            │ bars for each visible note  │  │
└───────────────────────────────────────────┘
```

### 9.2. Lane Types

| Lane | Data Source | Default |
|---|---|---|
| **Velocity** | Per-note velocity | Always visible |
| **Pitch Bend** | MIDI pitch bend (per-channel or per-note MPE) | Optional |
| **CC 1-127** | Any MIDI CC | Configurable |
| **Channel Pressure** | MIDI aftertouch | Optional |
| **Poly Pressure** | Per-key pressure | Optional |
| **MPE Timbre** | CC 74 (MPE standard) | Optional |
| **MPE Pressure** | Per-note pressure | Optional |
| **Automation** | Track/device parameter | Configurable |
| **Custom** | User-defined | Via Lua |

### 9.3. Curve Editing

```cpp
class ExpressionCurve {
public:
    struct Point {
        uint64_t ppqn;
        double value;           // Normalized 0.0 - 1.0
        InterpolationType interpolation;
        // For bezier:
        double control_x1, control_y1;
        double control_x2, control_y2;
    };

    // Add/move/remove points
    void add_point(const Point& point);
    void remove_point(uint64_t ppqn);
    void move_point(uint64_t ppqn, double new_value);
    void set_interpolation(uint64_t ppqn, InterpolationType type);

    // Evaluate at position
    double evaluate(uint64_t ppqn) const;

    // Rendering
    std::vector<Vec2> generate_line_strip(uint64_t start_ppqn,
                                           uint64_t end_ppqn,
                                           double pixels_per_ppqn_x,
                                           double pixels_per_value_y) const;

private:
    std::vector<Point> points_;
};

enum class InterpolationType {
    Step,           // Instant change at point (square wave)
    Linear,         // Straight line between points
    Bezier,         // Cubic bezier curve
    Smooth,         // Smooth Catmull-Rom spline
    EaseIn,         // Quadratic ease in
    EaseOut,        // Quadratic ease out
    EaseInOut,      // Smooth ease
    Hold            // Hold value until next point
};
```

### 9.4. Expression Drawing Tools

| Tool | Action |
|---|---|
| Pencil | Draw freehand curve (auto-simplify) |
| Line | Draw straight line (hold Shift for snapping) |
| Bezier | Place/edit bezier control points |
| Select | Select points for move/delete |
| Erase | Remove points on hover |
| Brush | Paint expression values (like velocity paint) |

---

## 10. Automation Integration

### 10.1. Automation Within Piano Roll

One of ARIA's key differentiators: automation clips can be edited directly within the piano roll, aligned with notes:

```
┌──────────────────────────────────────────┐
│  Piano Roll  │  🔄 Automation: Filter   │  ← Automation overlay toggle
├──────────────────────────────────────────┤
│         ┌──┐        ┌──┐               │
│      ┌──┘  └──┐  ┌──┘  └──┐            │  ← Note grid with
│  ┌──┘         └──┘         └──┐         │     automation overlay
│  │ ██  ██  ██  ██  ██  ██  ██ │         │
│  └──────────────────────────────┘         │
│  ┌────────────────────────────────────┐  │
│  │  Filter Cutoff  [─ ─ ─ ─ ─ ─ ─]  │  │  ← Automation lane
│  │   ────┐    ┌────┐    ┌───         │  │
│  │       └────┘    └────┘   └────    │  │
│  └────────────────────────────────────┘  │
└──────────────────────────────────────────┘
```

### 10.2. Automation Integration

```cpp
// Automation data is displayed as an optional overlay lane
// within the piano roll, sharing the same timeline.

// Users can:
// 1. Select an automation track to edit alongside notes
// 2. See automation parameter values at note positions
// 3. Edit automation points without leaving the piano roll
// 4. Copy/paste automation with notes (group operations)

class AutomationOverlay {
public:
    // Attach an automation track to the piano roll view
    void set_automation_track(AutomationTrack* track);
    AutomationTrack* automation_track() const;

    // Toggle overlay visibility
    void set_visible(bool visible);
    bool is_visible() const;

    // Editing
    void add_point(uint64_t ppqn, double value);
    void move_point(uint64_t from_ppqn, uint64_t to_ppqn, double value);

    // Sync with automation engine
    void sync_from_engine();
    void sync_to_engine();

private:
    AutomationTrack* track_ = nullptr;
    bool visible_ = false;
};
```

---

## 11. Ghost Notes

### 11.1. Ghost Note System

Ghost notes show notes from other clips/tracks as semi-transparent references:

```cpp
class GhostNoteSource {
public:
    enum class SourceType {
        SameTrack,      // Notes in adjacent clips on same track
        OtherTrack,     // Notes from a user-selected reference track
        Scale,          // Scale degree markers
        Chord,          // Current chord tones
        Custom          // User-specified reference
    };

    // Ghost notes are rendered at low opacity (15-30%)
    // They are not interactive (cannot select/edit)
    // They update in real-time as source changes
    // Color: desaturated version of source track color

    void set_source(SourceType type, track_id_t track = INVALID_TRACK);
    void set_opacity(float opacity);  // 0.0 - 1.0
    const std::vector<GhostNote>& get_ghost_notes() const;

private:
    SourceType type_ = SourceType::Scale;
    float opacity_ = 0.25f;
};
```

### 11.2. Ghost Note Types

| Source | Description | Use Case |
|---|---|---|
| **Adjacent clips** | Notes from clip before/after current clip | Smooth transitions |
| **Reference track** | Notes from a specific other track | Bass follows chords |
| **Scale** | Highlighted rows for current scale | Visual scale reference |
| **Chord** | Current chord tones highlighted | Harmony reference |
| **Drum map** | Drum note names/labels | Drum programming |
| **Audio peaks** | Transient markers from audio track | MIDI over audio |

### 11.3. Scale Ghost Display

```
Scale: C Minor (Bb, Eb, Ab)
┌──────────────────────────────────┐
│ C6  ────  ░░░░  ────  ░░░░     │  ← Ghost: scale tone (dim)
│ B5  ░░░░  ────  ░░░░  ────     │  ← Non-scale tone (very dim)
│ A5  ────  ░░░░  ────  ░░░░     │
│ G5  ────  ░░░░  ████  ░░░░     │  ← Scale tone with active note
│ F5  ░░░░  ────  ░░░░  ────     │
│ E5  ────  ░░░░  ────  ░░░░     │
│ D5  ░░░░  ████  ░░░░  ░░░░     │
│ C5  ████  ░░░░  ────  ░░░░     │  ← Active note
│ B4  ░░░░  ────  ░░░░  ────     │
│ A4  ────  ░░░░  ────  ░░░░     │
│ G4  ░░░░  ░░░░  ░░░░  ░░░░     │
│ F4  ░░░░  ────  ░░░░  ░░░░     │
│ E4  ────  ░░░░  ────  ░░░░     │
│ D4  ░░░░  ░░░░  ░░░░  ░░░░     │
│ C4  ────  ░░░░  ────  ░░░░     │
└──────────────────────────────────┘
     Beat: 1     2     3     4
```

---

## 12. Scale & Chord Tools

### 12.1. Scale System

```cpp
class ScaleSystem {
public:
    // Scale definitions
    struct Scale {
        std::string name;
        std::vector<int> intervals;  // Semitone intervals from root
        // Major: [0, 2, 4, 5, 7, 9, 11]
        // Minor: [0, 2, 3, 5, 7, 8, 10]
        // Pentatonic Major: [0, 2, 4, 7, 9]
    };

    // Common scales
    static const Scale MAJOR;
    static const Scale MINOR;
    static const Scale HARMONIC_MINOR;
    static const Scale MELODIC_MINOR;
    static const Scale PENTATONIC_MAJOR;
    static const Scale PENTATONIC_MINOR;
    static const Scale BLUES;
    static const Scale DORIAN;
    static const Scale PHRYGIAN;
    static const Scale LYDIAN;
    static const Scale MIXOLYDIAN;
    static const Scale LOCRIAN;
    static const Scale WHOLE_TONE;
    static const Scale DIMINISHED;
    static const Scale CHROMATIC;

    // Check if a note is in the current scale
    bool is_in_scale(uint8_t note, uint8_t root, const Scale& scale) const;

    // Get the closest scale note (for snap-to-scale)
    uint8_t snap_to_scale(uint8_t note, uint8_t root, const Scale& scale) const;

    // Get scale degree of a note (0 = root, -1 = not in scale)
    int scale_degree(uint8_t note, uint8_t root, const Scale& scale) const;

    // Get all scale tones as MIDI notes
    std::vector<uint8_t> scale_notes(uint8_t root, const Scale& scale,
                                      uint8_t octave_start = 0, uint8_t octave_end = 10) const;

private:
    Scale current_scale_ = MAJOR;
    uint8_t root_note_ = 60;  // C4
    bool enabled_ = false;
};
```

### 12.2. Chord Generator

```cpp
class ChordGenerator {
public:
    enum class ChordType {
        Major, Minor, Dim, Aug,
        Maj7, Min7, Dom7, Dim7, HalfDim7,
        Sus2, Sus4, Add9,
        Maj9, Min9, Dom9,
        Maj11, Min11,
        Maj13, Min13, Dom13,
        PowerChord,  // Root + 5th
        Custom       // User-defined
    };

    enum class Voicing {
        Close,           // Notes as close as possible
        Open,            // Spread across octaves
        Drop2,           // Drop 2 voicing
        Drop3,           // Drop 3 voicing
        Spread,          // Wide intervals
        Cluster,         // Dense cluster voicing
        Custom           // User-defined inversion
    };

    // Generate chord notes from root
    std::vector<uint8_t> generate(uint8_t root, ChordType type,
                                   Voicing voicing = Voicing::Close,
                                   uint8_t octave = 4);

    // Generate chord with inversions
    std::vector<uint8_t> generate_inversion(uint8_t root, ChordType type,
                                             int inversion);  // 0 = root, 1 = 1st, etc.

    // Generate chord progression
    struct Progression {
        std::vector<ChordType> chords;
        std::vector<uint8_t> roots;
    };
    Progression generate_progression(uint8_t key, const Scale& scale,
                                      const std::vector<int>& degrees);

    // Arpeggiate chord
    std::vector<uint64_t> arpeggiate(const std::vector<uint8_t>& notes,
                                      uint64_t start_ppqn,
                                      uint64_t note_duration_ppqn,
                                      ArpeggioPattern pattern);

    // Preview chord (play through audio engine)
    void preview_chord(const std::vector<uint8_t>& notes);
};
```

### 12.3. Chord Picker UI

```
┌─────────────────────────────────────┐
│  Chord Picker                        │
│                                     │
│  Root: [C] [C#] [D] [D#] ...      │  ← Root note selector
│                                     │
│  Type: [● Major] [○ Minor] [○ Dom7]│  ← Chord type buttons
│        [○ Maj7] [○ Min7] [○ Sus4]  │
│                                     │
│  Voicing: [Close] [Open] [Drop2]   │  ← Voicing selector
│                                     │
│  ┌────────────────────────────────┐ │
│  │  Preview: C E G     ●  ○  ○   │ │  ← Keyboard preview
│  │  ┌─┐┌─┐┌─┐                    │ │
│  │  │ ││ ││ │                    │ │
│  │  └─┘└─┘└─┘                    │ │
│  └────────────────────────────────┘ │
│                                     │
│  [Apply] [Apply to Selection]       │
└─────────────────────────────────────┘
```

### 12.4. Arpeggiator Integration

The piano roll includes a built-in arpeggiator that can generate patterns from held chords:

| Pattern | Description |
|---|---|
| **Up** | Notes ascend sequentially |
| **Down** | Notes descend sequentially |
| **Up/Down** | Ascend then descend |
| **Down/Up** | Descend then ascend |
| **Random** | Random order |
| **Chord** | All notes simultaneously |
| **As Played** | Played order preserved |
| **Forward/Back** | Ping-pong pattern |

---

## 13. Quantization & Humanization

### 13.1. Quantize Engine

```cpp
class QuantizeEngine {
public:
    struct QuantizeSettings {
        uint32_t grid_ppqn = 240;       // 16th notes
        double strength = 1.0;          // 0.0-1.0 (1.0 = full snap)
        bool snap_start = true;
        bool snap_end = false;
        double swing = 0.0;             // 0.0-1.0 swing amount
        uint32_t swing_grid = 480;      // Swing applied to this grid
        bool preserve_duration = true;  // Keep note length when moving start
    };

    // Quantize a collection of notes
    void quantize_notes(std::vector<Note*>& notes,
                        const QuantizeSettings& settings);

    // Quantize individual note
    Note quantize_note(const Note& note, const QuantizeSettings& settings);

    // Preview quantize (show result without applying)
    std::vector<Note> preview_quantize(const std::vector<Note*>& notes,
                                        const QuantizeSettings& settings);

private:
    // Internal: apply swing to grid
    double apply_swing(uint64_t ppqn, const QuantizeSettings& settings) const;
};
```

### 13.2. Humanize Engine

```cpp
class HumanizeEngine {
public:
    struct HumanizeSettings {
        bool randomize_start = true;
        bool randomize_duration = false;
        bool randomize_velocity = true;
        bool randomize_release = false;

        // Timing randomization (in PPQN)
        double timing_amount = 30;    // 0-120 PPQN standard deviation
        TimingCurve timing_curve = TimingCurve::Normal;

        // Velocity randomization
        double velocity_amount = 15;  // 0-63 standard deviation
        VelocityCurve velocity_curve = VelocityCurve::Normal;

        // Duration randomization
        double duration_amount = 10;  // 0-60 PPQN

        // Seed for reproducibility
        uint32_t seed = 0;            // 0 = random seed
    };

    void humanize_notes(std::vector<Note*>& notes,
                        const HumanizeSettings& settings);

    // Groove templates (extracted from real performances)
    struct GrooveTemplate {
        std::string name;
        std::vector<double> timing_offsets;  // PPQN offsets per grid position
        std::vector<uint8_t> velocity_multipliers;
    };

    void apply_groove(std::vector<Note*>& notes, const GrooveTemplate& groove);

    // Built-in groove templates
    static GrooveTemplate shuffle_swing(double amount);
    static GrooveTemplate latin();
    static GrooveTemplate funk();
    static GrooveTemplate hiphop();
    static GrooveTemplate human_feel();  // Subtle timing variations
};
```

### 13.3. Quantize Preview

```cpp
// When quantize is active, notes show a "pre-ghost" of their quantized position:
//
// Original note:  ████
// Quantized pos:  ░░░░████░░░░  ← Ghost shows where note will land
//                  ^ anchor point
//
// This allows users to see the effect before committing.
class QuantizePreview {
    void set_preview_notes(const std::vector<Note>& current,
                           const std::vector<Note>& quantized);
    void render_preview(GPUContext* ctx, const ViewTransform& view);
    void clear_preview();
};
```

---

## 14. Keyboard & Mouse

### 14.1. Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| **Navigation** | |
| `Home` | Go to project start |
| `End` | Go to project end |
| `Left/Right` | Move playhead by grid |
| `Up/Down` | Transpose selection by octave |
| `Shift+Up/Down` | Transpose selection by semitone |
| `Ctrl+Left/Right` | Move to next/previous note |
| **Selection** | |
| `Ctrl+A` | Select all |
| `Ctrl+D` | Deselect all |
| `I` | Invert selection |
| `Shift+Click` | Toggle note in selection |
| `Ctrl+Click` | Add to selection (marquee mode) |
| **Editing** | |
| `Delete` / `Backspace` | Delete selected notes |
| `Ctrl+C` | Copy selection |
| `Ctrl+V` | Paste at playhead |
| `Ctrl+X` | Cut selection |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` | Redo |
| `Ctrl+D` | Duplicate selection (offset by duration) |
| `Ctrl+L` | Legato (extend notes to next) |
| `Q` | Quantize selection |
| `Ctrl+Q` | Open quantize dialog |
| `H` | Humanize selection |
| `R` | Reverse selection |
| `T` | Transpose dialog |
| `M` | Mute/unmute selection |
| `L` | Lock/unlock selection |
| **Tools** | |
| `1` | Pencil |
| `2` | Select |
| `3` | Paint |
| `4` | Erase |
| `5` | Cut |
| `6` | Glue |
| `7` | Ramp (velocity) |
| `8` | Mute |
| `9` | Zoom |
| `0` | Measure |
| **Scale/Chord** | |
| `S` | Toggle scale highlighting |
| `Shift+S` | Change scale |
| `C` | Open chord generator |
| `Space` | Preview chord (with selection) |
| **View** | |
| `Ctrl++` | Zoom in |
| `Ctrl+-` | Zoom out |
| `Ctrl+0` | Fit all |
| `Ctrl+1` | Zoom to selection |
| `F` | Full screen piano roll |
| `Tab` | Cycle through docked panels |
| **Expression Lanes** | |
| `E` | Toggle expression lanes |
| `Shift+E` | Add expression lane |
| `Ctrl+E` | Edit expression lane binding |

### 14.2. Mouse Controls

| Input | Action |
|---|---|
| **Left Click** | Depends on active tool |
| **Left Drag** | Depends on active tool |
| **Shift+Left Drag** | Add to selection (marquee) |
| **Ctrl+Left Drag** | Move selection |
| **Alt+Left Drag** | Duplicate selection |
| **Right Click** | Context menu |
| **Right Drag** | Scroll (pan) |
| **Ctrl+Right Drag** | Zoom rectangle |
| **Middle Click** | Toggle tool (last two tools) |
| **Middle Drag** | Scroll (pan) |
| **Scroll Wheel** | Vertical scroll |
| **Shift+Scroll** | Horizontal scroll |
| **Ctrl+Scroll** | Horizontal zoom |
| **Ctrl+Shift+Scroll** | Vertical zoom |
| **Double Click** | Tool-specific action |
| **Hover** | Tooltip with note info |

### 14.3. MIDI Keyboard Input

```cpp
// When a MIDI keyboard is connected, notes can be input in real-time:
// 1. Press a key on MIDI controller → note appears at playhead position
// 2. Press and hold → note continues until key release
// 3. Note velocity = MIDI velocity
// 4. Notes are placed at the current grid snap position
// 5. If step-sequencing mode: playhead advances after each note

class MidiStepInput {
public:
    void enable(bool enabled);
    bool is_enabled() const;

    // Note input modes
    enum class InputMode {
        RealTime,       // Record as played
        Step,           // Step sequencer (advance after each note)
        Replace         // Replace note at cursor position
    };

    void set_input_mode(InputMode mode);
    void set_step_duration(uint32_t ppqn);  // Step size for step mode

    // Called when user presses a key on MIDI keyboard
    void on_midi_note_on(uint8_t note, uint8_t velocity);
    void on_midi_note_off(uint8_t note);

private:
    InputMode mode_ = InputMode::Step;
    uint32_t step_duration_ = 240;          // 16th note step
    uint64_t next_step_position_ = 0;
    NoteID pending_note_;                   // Note being held
};
```

---

## 15. Performance

### 15.1. Performance Targets

| Scenario | Target | Notes |
|---|---|---|
| 1,000 notes | 144 FPS | Typical project |
| 10,000 notes | 60+ FPS | Heavy project |
| 100,000 notes | 30+ FPS | Max target |
| Scrolling/zooming | Smooth (no stutter) | All note counts |
| Tool operation | < 16ms response | Create, move, delete |
| Quantization (10k notes) | < 100ms | Batch operation |

### 15.2. Optimization Strategies

```cpp
// 1. Spatial Indexing (grid-based)
//    - Divide the piano roll into a grid of cells (e.g., 1 bar × 1 octave)
//    - Only render notes in visible cells
//    - Only hit-test notes in cursor-adjacent cells
//    - Grid rebuild: O(n) for full rebuild, O(1) for cell updates

class SpatialGrid {
public:
    struct Cell {
        uint64_t start_ppqn, end_ppqn;
        uint8_t key_low, key_high;
        std::vector<Note*> notes;
    };

    void rebuild(const std::vector<Note>& notes);
    std::vector<Note*> query(const Rect& ppqn_rect) const;
    std::vector<Note*> query_cell(uint32_t col, uint32_t row) const;

private:
    static constexpr uint32_t CELL_WIDTH_PPQN = 3840;       // 1 bar
    static constexpr uint8_t CELL_HEIGHT_SEMITONES = 12;    // 1 octave
    std::unordered_map<uint64_t, Cell> cells_;  // Key: (col << 16) | row
};

// 2. GPU Instance Rendering
//    - All visible notes are rendered in ONE draw call (instanced)
//    - Instance data: position, size, color, opacity, flags
//    - Vertices are 4 vertices per note (2 triangles)
//    - GPU handles the rest

// 3. Persistent Vertex Mapping
//    - Vertex buffer is mapped persistently (no CPU→GPU copy per frame)
//    - Only update buffer data when notes change
//    - Use WRITE_NO_OVERWRITE flag when updating

// 4. LOD (Level of Detail)
//    - Zoomed far out: render notes as simple filled rectangles
//    - Zoomed in: render with rounded corners, shadows, detail
//    - Transition is continuous (GPU interpolates)

// 5. Async Note Processing
//    - Note operations (add, delete, modify) are debounced
//    - Spatial index rebuild: schedule on next idle frame
//    - Heavy operations (quantize 10k notes): run on background thread
```

### 15.3. Memory Budget

| Element | Memory (est.) | Notes |
|---|---|---|
| Note (per instance) | ~128 bytes | Includes MPE data |
| 10,000 notes | ~1.3 MB | Negligible |
| 100,000 notes | ~13 MB | Still small |
| Spatial grid | ~500 KB | For 100k notes |
| GPU vertex buffer | ~16 MB | Persistent mapped |
| GPU instance buffer | ~4 MB | 100k instances max |
| Texture atlas | ~8 MB | Icons, symbols, keyboard |
| Render targets | ~32 MB | 2x 4K RGBA8 buffers |
| **Total (100k notes)** | **~75 MB** | |

---

## 16. Multi-Monitor & Layout

### 16.1. Window Modes

```cpp
class PianoRollWindow {
public:
    // The piano roll can operate in three modes:
    enum class Mode {
        Inline,         // Part of the main window (docked panel)
        Floating,       // Separate floating window within main window
        Independent     // Completely separate OS window (multi-monitor)
    };

    void set_mode(Mode mode);
    Mode current_mode() const;

    // When in Independent mode, the piano roll:
    // 1. Creates its own OS window
    // 2. Has its own GPU swapchain
    // 3. Follows the main project cursor
    // 4. Syncs with main transport
  
    // Layout presets for Independent mode:
    // - Full: entire window is piano roll grid
    // - Classic: grid + velocity lane + 2 expression lanes
    // - Max: grid + 4 expression lanes + chord picker
    // - Compact: minimal (for second monitor)
    void set_layout_preset(LayoutPreset preset);
};
```

### 16.2. Layout Configuration

```
Inline Mode (default):
┌──────────────────────────────────────────────┐
│  [Project] [Mixer] [Piano Roll] [Browser]    │  ← Tab bar
├──────────────────────────────────────────────┤
│  ████████████████████████████████████████   │  ← Piano roll content
│  ████████████████████████████████████████   │
│  ████████████████████████████████████████   │
│  ████████████████████████████████████████   │
└──────────────────────────────────────────────┘

Independent Mode (second monitor):
┌──────────────────────────────────────────────┐
│  Piano Roll - [Track: Synth 1]  — □ ×       │  ← Independent window
├──────────────────────────────────────────────┤
│  ┌─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐ │
│  │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │  ← Full grid
│  │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
│  │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
│  │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │ │
│  └─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘ │
│  ┌─ Velocity ──────────────────────────────┐ │
│  │ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │ │
│  └──────────────────────────────────────────┘ │
│  ┌─ CC 74 (Timbre) ─────────────────────────┐ │
│  │ ──────┐    ┌───┐    ┌───┐    ┌──        │ │
│  │       └────┘   └────┘   └────┘          │ │
│  └──────────────────────────────────────────┘ │
└──────────────────────────────────────────────┘
```

---

## 17. API Reference

### 17.1. Public API

```cpp
class PianoRoll {
public:
    // Lifecycle
    void init(PianoRollConfig config);
    void destroy();

    // Data source
    void set_clip(MidiClip* clip);
    MidiClip* clip() const;

    // Note operations
    NoteID add_note(const Note& note);
    bool remove_note(NoteID id);
    bool update_note(NoteID id, const Note& note);
    Note* find_note(NoteID id);

    // Selection
    void select_note(NoteID id);
    void deselect_note(NoteID id);
    void select_all();
    void clear_selection();
    const std::vector<NoteID>& selected_notes() const;

    // Tools
    void set_active_tool(Tool tool);
    Tool active_tool() const;

    // View
    void set_viewport(const Rect& ppqn_rect);
    Rect viewport() const;
    void go_to_position(uint64_t ppqn);
    void go_to_note(NoteID id);
    void zoom_to_selection();
    void zoom_to_fit();

    // Grid & Snap
    void set_snap_enabled(bool enabled);
    bool snap_enabled() const;
    void set_snap_resolution(uint32_t ppqn);
    uint64_t snap_position(uint64_t ppqn) const;

    // Scale
    void set_scale(uint8_t root, const ScaleSystem::Scale& scale);
    void set_scale_enabled(bool enabled);
    bool scale_enabled() const;

    // Ghost notes
    void set_ghost_source(GhostNoteSource::SourceType type, track_id_t track);
    void set_ghost_opacity(float opacity);

    // Expression lanes
    void add_expression_lane(LaneType type, uint8_t cc_number = 0);
    void remove_expression_lane(uint32_t index);
    void clear_expression_lanes();

    // Automation overlay
    void set_automation_overlay(AutomationTrack* track);
    void set_automation_visible(bool visible);

    // Window mode
    void set_window_mode(PianoRollWindow::Mode mode);
    PianoRollWindow::Mode window_mode() const;

    // Keyboard input
    MidiStepInput& step_input();

    // Undo
    void push_undo_state(const std::string& description);
    void undo();
    void redo();
};
```

### 17.2. Configuration

```cpp
struct PianoRollConfig {
    // Appearance
    Color background_color = Color(0x1A, 0x1A, 0x1A);
    Color grid_color = Color(0x2A, 0x2A, 0x2A);
    Color beat_color = Color(0x33, 0x33, 0x33);
    Color bar_color = Color(0x3D, 0x3D, 0x3D);
    Color cursor_color = Color(0xFF, 0x7A, 0x00);
    Color selection_color = Color(0xFF, 0xFF, 0xFF, 0x30);

    // Note defaults
    uint32_t default_duration_ppqn = 240;    // 16th note
    uint8_t default_velocity = 100;
    uint8_t default_channel = 0;

    // Grid
    uint32_t default_grid_ppqn = 240;        // 16th notes
    bool snap_enabled_by_default = true;

    // Performance
    uint32_t max_notes = 100000;
    uint32_t target_fps = 144;
    bool enable_gpu_acceleration = true;
    bool enable_async_processing = true;

    // Layout
    PianoRollWindow::Mode default_window_mode = PianoRollWindow::Mode::Inline;
    uint32_t min_note_width_px = 2;
    float ghost_opacity_default = 0.25f;
};
```

---

## 18. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-PR-001 | Note Data Model & Collection | 🔲 Pending |
| RFC-PR-002 | GPU Render Pipeline & Shaders | 🔲 Pending |
| RFC-PR-003 | Viewport & Spatial Indexing | 🔲 Pending |
| RFC-PR-004 | Grid & Snap System | 🔲 Pending |
| RFC-PR-005 | Tool System (Pencil, Select, Paint, Erase) | 🔲 Pending |
| RFC-PR-006 | Cut, Glue & Split Operations | 🔲 Pending |
| RFC-PR-007 | Selection System & Handles | 🔲 Pending |
| RFC-PR-008 | Velocity Editing & Ramp | 🔲 Pending |
| RFC-PR-009 | Expression Lanes (CC, Pitch Bend, Pressure) | 🔲 Pending |
| RFC-PR-010 | Automation Overlay | 🔲 Pending |
| RFC-PR-011 | Ghost Notes & Reference System | 🔲 Pending |
| RFC-PR-012 | Scale System & Scale Highlighting | 🔲 Pending |
| RFC-PR-013 | Chord Generator & Arpeggiator | 🔲 Pending |
| RFC-PR-014 | Quantization Engine | 🔲 Pending |
| RFC-PR-015 | Humanization & Groove Templates | 🔲 Pending |
| RFC-PR-016 | MPE Editing & Display | 🔲 Pending |
| RFC-PR-017 | MIDI Step Input & Real-Time Recording | 🔲 Pending |
| RFC-PR-018 | Multi-Monitor & Layout System | 🔲 Pending |
| RFC-PR-019 | Keyboard Shortcuts & Accessibility | 🔲 Pending |
| RFC-PR-020 | LOD & Performance Optimization | 🔲 Pending |
| RFC-PR-021 | Drum Mode (grid-based note placement) | 🔲 Pending |
| RFC-PR-022 | Note Labels & Annotations | 🔲 Pending |
| RFC-PR-023 | Undo/Redo for Piano Roll Operations | 🔲 Pending |
| RFC-PR-024 | Print / Export MIDI from Piano Roll | 🔲 Pending |

---

## Appendix A: Drum Mode

Drum mode is an alternative piano roll layout optimized for drum programming:

```
Drum Mode Layout:
┌──────────────────────────────────────────────┐
│  Kick    │ ██  ██  ██  ██  ██  ██  ██  ██ │  ← One row per drum sound
│  Snare   │ ░░  ██  ░░  ██  ░░  ██  ░░  ██ │
│  HiHat   │ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ │
│  Clap    │ ░░  ░░  ░░  ░░  ██  ░░  ░░  ░░ │
│  Tom Hi  │ ░░  ░░  ░░  ░░  ░░  ░░  ░░  ░░ │
│  Tom Lo  │ ░░  ░░  ░░  ░░  ░░  ░░  ░░  ░░ │
│  Rim     │ ░░  ░░  ░░  ░░  ░░  ░░  ░░  ░░ │
│  Crash   │ ░░  ░░  ░░  ░░  ██  ░░  ░░  ░░ │
└──────────────────────────────────────────────┘
     Beat    1      2      3      4

Key features:
  - Rows labeled with drum names (configurable mapping)
  - Grid by 16th notes (or user preference)
  - Velocity shown as note size (larger = louder)
  - Right-click note → change drum sound
  - Pattern browser with built-in drum patterns
  - Swing per-row (e.g., hi-hats swung, kick straight)
```

## Appendix B: Visual Style Guide

```
Piano Roll Visual Identity:

  Background:    #1A1A1A (dark charcoal)
  Grid lines:    #2A2A2A (slightly lighter)
  Beat lines:    #333333 (subtle accent)
  Bar lines:     #3D3D3D (stronger accent)
  Playhead:      #FF7A00 (ARIA orange accent)
  
  Note bodies:
    Track color at 80% opacity, overlaid with
    velocity gradient from 15% (bottom) to 100% (top)
  
  Selection:     White outline, 2px, rounded corners
  Ghost notes:   Same as note color at 25% opacity
  
  Velocity bars:
    Low (0-32):    #4A9EFF (blue)
    Mid (33-96):   #7AFF7A (green)  
    High (97-127): #FF7A4A (orange → red)
  
  Expression curves:
    Pitch bend:    #FF7A00 (orange)
    CC data:       #7AC0FF (light blue)
    Pressure:      #FF7A7A (red)
    
  Scale tones:
    Root:          very subtle highlight
    Scale degrees: subtle dot marker on keyboard
    Non-scale:     dimmed slightly
    
  Animations:
    Note create:   100ms ease-out (scale from 0 to full size)
    Note delete:   80ms ease-in (scale to 0 + fade)
    Selection:     150ms ease (instant feel)
    Scroll:        momentum-based (0.5s decay)
    Zoom:          anchored at cursor position
```
