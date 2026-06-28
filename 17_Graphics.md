# ARIA DAW — Graphics Engine Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **Engine**: Custom GPU-rendered widget system
> **Backend**: WebGPU + Skia

---

## Table of Contents

1. [Overview](#1-overview)
2. [Widget System](#2-widget-system)
3. [GPU Widget Rendering](#3-gpu-widget-rendering)
4. [Layout Engine](#4-layout-engine)
5. [Animations](#5-animations)
6. [Specialized Rendering](#6-specialized-rendering)
7. [Canvas Elements](#7-canvas-elements)
8. [Performance](#8-performance)
9. [API Reference](#9-api-reference)
10. [RFC Index](#10-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Graphics Engine provides all visual widget rendering for ARIA DAW. Unlike traditional DAWs that use OS-native controls or CPU-rendered frameworks (Qt, GDI), ARIA renders EVERY visual element on the GPU — buttons, sliders, knobs, faders, meters, waveforms, the piano roll, and all canvas-based editors.

### 1.2. Design Philosophy

```
NO:
  ✗ OS-native controls (button, listbox, etc.)
  ✗ GDI / win32 rendering
  ✗ Qt widgets (QPushButton, QSlider, etc.)
  ✗ CPU rasterization of UI elements
  ✗ Bitmap caching with CPU updates

YES:
  ✓ GPU-accelerated vector rendering
  ✓ Shader-based color/theming (no redraw needed on theme change)
  ✓ Instanced geometry for repeated elements
  ✓ Declarative layout (flexbox-like)
  ✓ Animation as first-class property
```

### 1.3. Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  TypeScript Component Tree                                   │
│  (declarative, reactive)                                     │
├──────────────────────────────────────────────────────────────┤
│  C++ Graphics Bridge (FFI)                                   │
│  Converts component tree → GPU draw commands                │
├──────────────────────────────────────────────────────────────┤
│  Widget Primitives                                           │
│  Rect, Text, Image, Path, Shadow, Blur, Glow                │
├──────────────────────────────────────────────────────────────┤
│  Layout Engine                                               │
│  Flexbox, Grid, Dock, Stack                                 │
├──────────────────────────────────────────────────────────────┤
│  GPU Renderer                                                │
│  Skia + WebGPU                                               │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Widget System

### 2.1. Widget Primitives

```cpp
class Widget {
public:
    Widget(WidgetID id);
    virtual ~Widget() = default;
    
    // Identity
    WidgetID id() const;
    std::string type() const;
    
    // Geometry
    void set_bounds(const Rect& bounds);
    Rect bounds() const;
    void set_position(float x, float y);
    void set_size(float width, float height);
    
    // Layout
    void set_layout(const LayoutParams& params);
    LayoutParams layout() const;
    
    // Visual
    void set_visible(bool visible);
    bool is_visible() const;
    void set_opacity(float opacity);
    float opacity() const;
    void set_transform(const Transform& transform);
    
    // State
    void set_enabled(bool enabled);
    bool is_enabled() const;
    void set_hovered(bool hovered);
    bool is_hovered() const;
    void set_pressed(bool pressed);
    bool is_pressed() const;
    void set_focused(bool focused);
    bool is_focused() const;
    
    // Parent/child
    void set_parent(Widget* parent);
    Widget* parent() const;
    void add_child(std::unique_ptr<Widget> child);
    void remove_child(WidgetID id);
    const std::vector<Widget*>& children() const;
    
    // Rendering
    virtual void render(SkiaCanvas* canvas) = 0;
    virtual void render_gpu(CommandBuffer* cmd) {}
    
    // Hit testing
    virtual bool hit_test(float x, float y) const;
    
    // Animation
    void animate(const std::string& property,
                 const AnimationTarget& target,
                 const AnimationConfig& config);

protected:
    WidgetID id_;
    std::string type_;
    Rect bounds_;
    LayoutParams layout_;
    bool visible_ = true;
    float opacity_ = 1.0f;
    Transform transform_;
    bool enabled_ = true;
    bool hovered_ = false;
    bool pressed_ = false;
    bool focused_ = false;
    
    Widget* parent_ = nullptr;
    std::vector<std::unique_ptr<Widget>> children_;
};
```

### 2.2. Built-in Widgets

```cpp
// All widgets are GPU-rendered vectors, not bitmaps:

class RectWidget : public Widget {
    // Solid rectangle with rounded corners
    // Color, border, shadow
    void set_fill_color(const Color& color);
    void set_border(const Border& border);
    void set_corner_radius(float radius);
    void set_shadow(const Shadow& shadow);
};

class TextWidget : public Widget {
    // GPU-rendered text from glyph atlas
    void set_text(const std::string& text);
    void set_font(const FontDesc& font);
    void set_color(const Color& color);
    void set_alignment(TextAlignment align);
    void set_wrapping(TextWrap wrap);
    void set_ellipsis(const std::string& ellipsis);
};

class ImageWidget : public Widget {
    // GPU texture from atlas
    void set_image(const ImageRef& image);
    void set_sizing(ImageSizing sizing);  // fill, fit, stretch
    void set_tint(const Color& color);
};

class SvgWidget : public Widget {
    // Vector SVG rendered on GPU
    void set_svg(const SvgRef& svg);
    void set_color(const Color& color);
    void set_stroke_width(float width);
};

class PathWidget : public Widget {
    // Arbitrary GPU vector path
    void set_path(const Path2D& path);
    void set_fill(const Paint& fill);
    void set_stroke(const Paint& stroke);
};

class ButtonWidget : public RectWidget {
    // Interactive button
    void set_label(const std::string& label);
    void set_icon(const IconRef& icon);
    void set_variant(ButtonVariant variant);
    void set_on_click(std::function<void()> callback);
    // States: default, hover, pressed, disabled
};

class SliderWidget : public Widget {
    // Horizontal/vertical slider
    void set_value(double value);
    void set_range(double min, double max);
    void set_step(double step);
    void set_orientation(Orientation orientation);
    void set_on_change(std::function<void(double)> callback);
    // Track, fill, thumb — all GPU rendered
};

class KnobWidget : public Widget {
    // Rotary knob/encoder
    void set_value(double value);
    void set_range(double min, double max);
    void set_step(double step);
    void set_arc_range(double start_deg, double end_deg);
    void set_on_change(std::function<void(double)> callback);
    // Arc, indicator, center dot — all GPU rendered
};

class FaderWidget : public Widget {
    // Vertical fader (mixer channel strip)
    void set_value(double db);
    void set_range(double min_db, double max_db);
    void set_track(const FaderTrack& track);
    void set_cap(const FaderCap& cap);
    void set_on_change(std::function<void(double)> callback);
};

class MeterWidget : public Widget {
    // Level meter (peak/RMS)
    void set_levels(const MeterLevels& levels);
    void set_meter_type(MeterType type);
    void set_orientation(Orientation orientation);
    void set_hold_peak(bool hold);
    void reset_hold_peak();
};

class WaveformWidget : public Widget {
    // Audio waveform display
    void set_waveform_data(const WaveformData* data);
    void set_colors(const WaveformColors& colors);
    void set_time_range(uint64_t start_sample, uint64_t end_sample);
    void set_show_grid(bool show);
};

class ScrollWidget : public Widget {
    // Scrollable container
    void set_content_size(float width, float height);
    void set_scroll_position(float x, float y);
    void scroll_to(float x, float y, bool animated);
    // Smooth momentum scrolling
    // Auto-hide scrollbars
};

class DockWidget : public Widget {
    // Docking container (split, tab, float)
    void set_layout(const DockLayout& layout);
    void add_panel(const std::string& id, std::unique_ptr<Widget> panel);
    void remove_panel(const std::string& id);
    void split(Direction direction);
};
```

---

## 3. GPU Widget Rendering

### 3.1. Render Passes

```cpp
// Widgets are rendered in specific order and passes:

enum class RenderPass {
    Background,      // Window backgrounds, panel fills
    Shadows,         // Drop shadows (blur pass)
    Content,         // Main content (tracks, clips, notes)
    Overlays,        // Selections, drag indicators
    Tooltips,        // Tooltips, popups, context menus
    Cursor           // Mouse cursor (if custom)
};
```

### 3.2. Widget Batching

```cpp
// Widgets of the same type are batched into GPU instanced draws:
//
// Buttons (200 instances):
//   - 1 vertex buffer (4 vertices × 200)
//   - 1 instance buffer (position, size, color, state)
//   - 1 draw call
//
// Text labels (500 instances):
//   - 1 glyph atlas
//   - Batched by font + size + color
//   - ~50 draw calls (not 500)
//
// Meter bars (32 instances):
//   - 1 vertex buffer
//   - 1 instance buffer (position, level, color)
//   - 1 draw call
```

### 3.3. Dirty Region Tracking

```cpp
class DirtyTracker {
public:
    void mark_dirty(const Rect& region);
    void mark_all_dirty();
    
    Rect dirty_region() const;
    bool is_dirty() const;
    
    // Widgets can return whether they changed:
    // false = skip render (use cached framebuffer)
    // true = re-render this widget
    bool widget_needs_redraw(WidgetID id) const;
    
    void clear();
    
private:
    Rect dirty_rect_;
    std::unordered_set<WidgetID> dirty_widgets_;
};
```

---

## 4. Layout Engine

### 4.1. Layout Types

```cpp
// GPU-computed layout using the same primitives the
// UI Framework uses, but implemented in C++:

enum class LayoutType {
    FlexRow,         // Horizontal flexbox
    FlexColumn,      // Vertical flexbox
    Grid,            // 2D grid
    Dock,            // Docking split/tab
    Stack,           // Z-order stack
    Absolute,        // Absolute positioning
    Scroll,          // Scrollable content
};

struct LayoutParams {
    LayoutType type = LayoutType::FlexColumn;
    
    // Flex
    float flex_grow = 0;
    float flex_shrink = 1;
    float flex_basis = 0;
    
    // Alignment
    Alignment horizontal_align = Alignment::Start;
    Alignment vertical_align = Alignment::Center;
    
    // Spacing
    float gap = 0;
    EdgeInsets padding = {0, 0, 0, 0};
    EdgeInsets margin = {0, 0, 0, 0};
    
    // Size
    float min_width = 0;
    float max_width = FLT_MAX;
    float min_height = 0;
    float max_height = FLT_MAX;
    float preferred_width = 0;    // 0 = auto
    float preferred_height = 0;   // 0 = auto
};
```

### 4.2. Layout Computation

```cpp
class LayoutEngine {
public:
    // Compute layout for entire widget tree
    void compute(Widget* root, const Size& viewport);
    
    // Compute layout for subtree
    void compute_subtree(Widget* node);
    
    // Mark subtree as needing layout
    void invalidate(Widget* node);
    
    // Layout passes:
    // 1. Measure pass: determine preferred sizes
    // 2. Placement pass: assign final positions
    // 3. Overflow pass: handle overflow (scroll/clip)
    
    // GPU acceleration:
    // Layout is computed on CPU (complex recursion),
    // but each widget's final position/size is uploaded
    // to GPU as instance data for rendering.
    
    // Layout transitions:
    // When a widget changes size/position, the transition
    // is animated on the GPU (no CPU involvement).
};
```

---

## 5. Animations

### 5.1. Animation Properties

```cpp
// Every visual property is animatable:
//
// Position:    x, y (translate)
// Size:        width, height (scale)
// Visual:      opacity, color, border-radius
// Transform:   rotate, scale, skew
// State:       hover, press, focus, active
// Layout:      flex-grow, gap, padding
// Value:       slider, knob, fader values

struct AnimationConfig {
    float duration_ms = 150;
    EasingCurve easing = EasingCurve::EaseOut;
    float delay_ms = 0;
    bool yoyo = false;
    bool loop = false;
    std::function<void()> on_complete;
    
    // GPU-side animation:
    // Animation parameters are uploaded to GPU
    // as a uniform buffer, and the GPU interpolates
    // the value per-frame with zero CPU involvement.
    enum class Execution {
        CPU,    // CPU interpolated (legacy)
        GPU     // GPU interpolated (preferred)
    };
    Execution execution = Execution::GPU;
};
```

### 5.2. Easing Curves

```cpp
enum class EasingCurve {
    Linear,
    
    // Quadratic
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    
    // Cubic
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    
    // Quart
    EaseInQuart,
    EaseOutQuart,
    EaseInOutQuart,
    
    // Quint
    EaseInQuint,
    EaseOutQuint,
    EaseInOutQuint,
    
    // Elastic
    EaseOutElastic,     // Bouncy overshoot
    EaseInElastic,
    EaseInOutElastic,
    
    // Back (overshoot)
    EaseOutBack,        // Slight overshoot then settle
    EaseInBack,
    EaseInOutBack,
    
    // Bounce
    EaseOutBounce,
    EaseInBounce,
    
    // Smooth step
    SmoothStep,
    SmootherStep,
    
    // Custom (cubic bezier)
    CubicBezier
};
```

### 5.3. GPU Animation Pipeline

```cpp
// GPU-side animation flow:
//
// 1. CPU uploads AnimationUniforms:
//    struct AnimationUniforms {
//        float start_value;
//        float end_value;
//        float start_time;     // GPU timestamp
//        float duration;
//        float easing_params[4];  // Bezier control points
//    };
//
// 2. GPU interpolates per-frame in vertex shader:
//    float t = (current_time - start_time) / duration;
//    float eased_t = apply_easing(t, curve);
//    float value = mix(start_value, end_value, eased_t);
//
// 3. GPU applies interpolated value directly:
//    - Position → model matrix translation
//    - Opacity → alpha blending factor
//    - Color → fragment shader lerp
//    - Size → vertex position scaling
//
// Result: Animations run at full GPU frame rate
// with ZERO CPU cost after initialization.
```

---

## 6. Specialized Rendering

### 6.1. Waveform Rendering

```cpp
class WaveformRenderer {
public:
    struct WaveformVertex {
        float x, y;
        float peak_y;
        float min_y;
    };
    
    // Generate waveform vertex buffer
    Buffer* generate_vertices(const WaveformData* data,
                               const Rect& viewport);
    
    // Update when zoom/scroll changes
    void update_viewport(const Rect& new_viewport);
    
    // Render
    void render(CommandBuffer* cmd, const WaveformStyle& style);
    
    // LOD:
    // Zoomed out: peak-only (single line)
    // Medium zoom: peak + min (filled shape)
    // Zoomed in: full sample-level detail
    enum class Lod { Peak, PeakMin, Full };
    Lod current_lod(const Rect& viewport) const;
    
    // Performance:
    // 10-minute track at 44.1kHz = ~26M samples
    // Peak LOD: 1024 vertices per visible region
    // Full LOD: viewport-limited to visible pixels
};
```

### 6.2. Piano Roll Rendering

```cpp
class PianoRollRenderer {
public:
    // Note instances (uploaded once, updated on change)
    struct NoteInstance {
        float x, y;           // Position
        float width, height;  // Size
        uint32_t color;       // Packed RGBA
        float velocity;       // 0-1
        uint32_t flags;       // selected, muted, ghost
    };
    
    Buffer* note_instance_buffer_;
    
    // Grid lines (uploaded once, updated on zoom)
    struct GridLine {
        float x0, y0, x1, y1;
        uint32_t color;
        float alpha;
    };
    
    // Render passes:
    // 1. Background + grid (single draw call)
    // 2. Ghost notes (alpha blend, non-interactive)
    // 3. Note bodies (instanced, ~1 draw call)
    // 4. Note velocity overlays (same instance, different color)
    // 5. Selection outlines (separate pass)
    // 6. Playhead + loop markers
    // 7. Keyboard (textured quads from atlas)
    // 8. Expression lanes (curves below grid)
    
    void render(CommandBuffer* cmd, const Rect& viewport);
    void update_note_data(const std::vector<NoteInstance>& notes);
};
```

### 6.3. Spectrogram Rendering

```cpp
class SpectrogramRenderer {
public:
    // FFT data to texture
    // Each frame: compute FFT → upload to texture row
    // Scrolling: shift texture up, add new row at bottom
    
    void add_fft_frame(const float* magnitudes, uint32_t bin_count);
    void render(CommandBuffer* cmd, const Rect& viewport);
    
    // GPU compute shader for FFT → texture conversion:
    // - Map magnitude to color gradient
    // - Apply frequency scale (log or linear)
    // - Interpolate between frames for smoothness
};
```

---

## 7. Canvas Elements

### 7.1. Canvas Types

```cpp
// ARIA has multiple specialized canvas types:

enum class CanvasType {
    // Standard UI canvas (widgets, panels)
    UI,
    
    // Timeline/arrangement canvas
    Timeline,
    
    // Piano roll canvas (vector MIDI editor)
    PianoRoll,
    
    // Waveform display canvas
    Waveform,
    
    // Automation lane canvas
    Automation,
    
    // Spectrogram canvas
    Spectrogram,
    
    // Mixer canvas (channel strips)
    Mixer,
    
    // Session view canvas (clip grid)
    Session
};
```

### 7.2. Canvas Features

```cpp
// Each canvas type has specific features:
//
// UI Canvas:
//   - Widget tree rendering
//   - Flexbox layout
//   - Animation system
//   - Accessibility
//
// Timeline Canvas:
//   - Horizontal timeline
//   - Track lanes
//   - Clip rendering (audio waveform, MIDI notes)
//   - Automation overlays
//   - Playhead
//   - Markers
//   - Time ruler
//   - Zoom/scroll
//
// Piano Roll Canvas:
//   - Vector note rendering
//   - Expression lanes
//   - Velocity editor
//   - Ghost notes
//   - Scale highlighting
//
// Waveform Canvas:
//   - LOD rendering
//   - Multi-channel (stereo, surround)
//   - Spectral overlays
//   - Transient markers
//   - Warp markers
//
// Mixer Canvas:
//   - Vertical channel strips
//   - Meter bars (peak + RMS)
//   - Fader control
//   - Routing visualization
```

---

## 8. Performance

### 8.1. Widget Performance

| Widget Type | Draw Calls | Vertices | GPU Time |
|---|---|---|---|
| Button (1) | 1 | 4 | 0.01ms |
| Button (200) | 1 (instanced) | 800 | 0.05ms |
| Slider | 2 | 12 | 0.02ms |
| Knob | 3 | 24 | 0.03ms |
| Meter bar (32) | 1 (instanced) | 128 | 0.03ms |
| Text label | 1 (glyph run) | ~50 | 0.02ms |
| Waveform (track) | 1 | ~1024 | 0.1ms |
| Piano roll note | 1 (instanced) | 4 per note | 0.001ms per 1000 |
| Automation curve | 1 | ~200 | 0.05ms |

### 8.2. Full Frame Budget

```
Empty project (144 FPS target):
  Background:           0.1ms
  Grid:                 0.05ms
  Toolbar:              0.1ms
  Status bar:           0.05ms
  Browser panel:        0.2ms
  ──────────────────────────
  Total:                ~0.5ms (13x headroom)

Production project (32 tracks, 144 FPS target):
  UI widgets:           0.5ms
  Track headers:        0.1ms
  Waveforms (32):       0.3ms
  Mixer strips (32):    0.3ms
  Automation:           0.1ms
  Overlays:             0.1ms
  Post-processing:      0.2ms
  ──────────────────────────
  Total:                ~1.6ms (4x headroom)

Large project (100 tracks, 60 FPS target):
  UI widgets:           1ms
  Track headers:        0.3ms
  Waveforms (100):      1ms
  Mixer strips (100):   1ms
  Automation:           0.3ms
  Overlays:             0.2ms
  Post-processing:      0.5ms
  ──────────────────────────
  Total:                ~4.3ms (3.8x headroom)
```

---

## 9. API Reference

### 9.1. Public API

```cpp
class GraphicsEngine {
public:
    // Canvas management
    Canvas* create_canvas(CanvasType type, Widget* root);
    void destroy_canvas(Canvas* canvas);
    Canvas* get_canvas(CanvasType type);
    
    // Widget factory
    template<typename T, typename... Args>
    T* create_widget(Args&&... args);
    void destroy_widget(Widget* widget);
    
    // Specialized renderers
    WaveformRenderer* waveform();
    PianoRollRenderer* piano_roll();
    SpectrogramRenderer* spectrogram();
    
    // Layout
    LayoutEngine& layout();
    
    // Animations
    AnimationEngine& animations();
    
    // Dirty tracking
    DirtyTracker& dirty();
    
    // Render a frame
    void render_frame(SwapChain* swapchain, const Rect& viewport);
};
```

---

## 10. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-GFX-001 | Widget Primitives (Rect, Text, Image, Path) | 🔲 Pending |
| RFC-GFX-002 | Button Widget (variants, states) | 🔲 Pending |
| RFC-GFX-003 | Slider & Knob Widgets | 🔲 Pending |
| RFC-GFX-004 | Fader Widget (mixer style) | 🔲 Pending |
| RFC-GFX-005 | Meter Widget (peak/RMS) | 🔲 Pending |
| RFC-GFX-006 | Waveform Display Widget | 🔲 Pending |
| RFC-GFX-007 | Scroll Widget (momentum, auto-hide) | 🔲 Pending |
| RFC-GFX-008 | Dock Widget (split, tab, float) | 🔲 Pending |
| RFC-GFX-009 | GPU Instance Batching | 🔲 Pending |
| RFC-GFX-010 | Dirty Region Tracking | 🔲 Pending |
| RFC-GFX-011 | Flexbox Layout Engine | 🔲 Pending |
| RFC-GFX-012 | GPU-Side Animation | 🔲 Pending |
| RFC-GFX-013 | Easing Curves (GPU shader) | 🔲 Pending |
| RFC-GFX-014 | Waveform LOD Rendering | 🔲 Pending |
| RFC-GFX-015 | Piano Roll GPU Renderer | 🔲 Pending |
| RFC-GFX-016 | Spectrogram Compute Shader | 🔲 Pending |
| RFC-GFX-017 | Canvas Types & Specialization | 🔲 Pending |
| RFC-GFX-018 | Theme-Change Without Redraw | 🔲 Pending |
| RFC-GFX-019 | Touch & Pen Input on Canvas | 🔲 Pending |
| RFC-GFX-020 | Widget DevTools (inspect, profile) | 🔲 Pending |

---

## Appendix A: Widget Memory

| Widget | Memory |
|---|---|
| RectWidget | 128 bytes |
| TextWidget | 256 bytes (incl. string) |
| SliderWidget | 192 bytes |
| KnobWidget | 192 bytes |
| FaderWidget | 256 bytes |
| MeterWidget | 256 bytes |
| WaveformWidget | 512 bytes (+ GPU buffer) |
| ScrollWidget | 320 bytes |
| DockWidget | 1024 bytes (complex children) |
| **Average widget** | **~200 bytes** |
| **10,000 widgets** | **~2 MB** |

## Appendix B: Rendering Fallbacks

```
GPU available:
  → Full GPU rendering (WebGPU + Skia)
  
GPU available but no WebGPU support:
  → Skia Software Rendering (CPU, reduced performance)
  
No GPU:
  → Skia Software Rendering (CPU only, 30 FPS target)
  → Reduced effects (no blur, no shadows)
  → Minimum resolution: 1280×720
```
