# ARIA DAW — GPU Rendering Specification

> **Version**: 0.1
> **Status**: Draft
> **Last Updated**: 2026-06-27
> **API**: WebGPU (Vulkan / Metal / DirectX 12)
> **2D Library**: Skia

---

## Table of Contents

1. [Overview](#1-overview)
2. [WebGPU Abstraction](#2-webgpu-abstraction)
3. [Skia Integration](#3-skia-integration)
4. [Render Pipeline](#4-render-pipeline)
5. [Shaders](#5-shaders)
6. [Texture Management](#6-texture-management)
7. [Post-Processing](#7-post-processing)
8. [HiDPI Support](#8-hidpi-support)
9. [Multi-Window Rendering](#9-multi-window-rendering)
10. [Performance](#10-performance)
11. [API Reference](#11-api-reference)
12. [RFC Index](#12-rfc-index)

---

## 1. Overview

### 1.1. Purpose

The Rendering Engine is the foundation of all visual output in ARIA DAW. It provides hardware-accelerated 2D rendering via WebGPU (Vulkan/Metal/DirectX 12) with Skia as the high-level 2D graphics library. Every pixel — from UI widgets to waveform displays to the piano roll — passes through the GPU.

### 1.2. Design Goals

| Goal | Target |
|---|---|
| Frame rate | 144 FPS at 1080p |
| Resolution support | 720p to 5K (Retina/HiDPI) |
| GPU API | WebGPU (Vulkan/Metal/DX12) |
| 2D library | Skia (Chrome-level quality) |
| Multi-window | Independent GPU contexts per window |
| Anti-aliasing | 4x MSAA or better |

### 1.3. Rendering Stack

```
┌──────────────────────────────────────────────────────┐
│                    Application                         │
│  (UI Framework, Piano Roll, Waveforms, etc.)          │
├──────────────────────────────────────────────────────┤
│                  Skia Canvas                           │
│  (High-level 2D: paths, text, images, effects)        │
├──────────────────────────────────────────────────────┤
│               WebGPU Abstraction                       │
│  (Device, SwapChain, Pipeline, Buffers, Textures)      │
├──────────────────────────────────────────────────────┤
│         ┌──────────┬──────────┬──────────┐            │
│         │ Vulkan   │  Metal   │ DirectX  │            │
│         │          │          │    12    │            │
│         └──────────┴──────────┴──────────┘            │
└──────────────────────────────────────────────────────┘
```

---

## 2. WebGPU Abstraction

### 2.1. GPU Device Management

```cpp
class GPUDevice {
public:
    // Initialize GPU device
    bool init(const GPUConfig& config);
    void shutdown();
    
    // Create swap chain for window
    SwapChain* create_swapchain(void* native_window,
                                 uint32_t width, uint32_t height);
    
    // Create GPU resources
    Buffer* create_buffer(const BufferDesc& desc);
    Texture* create_texture(const TextureDesc& desc);
    Sampler* create_sampler(const SamplerDesc& desc);
    ShaderModule* create_shader(const ShaderDesc& desc);
    RenderPipeline* create_render_pipeline(const PipelineDesc& desc);
    ComputePipeline* create_compute_pipeline(const ComputePipelineDesc& desc);
    
    // Command submission
    void submit(CommandBuffer* cmd);
    void present(SwapChain* swapchain);
    
    // Wait for GPU
    void wait_idle();
    
    // Device info
    struct DeviceInfo {
        std::string name;
        std::string vendor;
        uint32_t dedicated_memory_mb;
        uint32_t max_texture_size;
        uint32_t max_buffer_size;
        bool supports_raytracing;
        bool supports_compute_shaders;
        float timestamp_period;          // ns
    };
    DeviceInfo info() const;
    
    // Feature detection
    bool supports_feature(const std::string& feature) const;

private:
    // Backend-specific implementation
    // wgpu::Device on WebGPU native
    // VkDevice on Vulkan
    // id<MTLDevice> on Metal
    // ID3D12Device on DirectX 12
    void* backend_device_;
};

struct GPUConfig {
    bool enable_debug_layer = false;
    bool enable_gpu_validation = false;
    bool prefer_low_power = false;
    uint32_t max_texture_size = 16384;
    bool enable_msaa = true;
    uint32_t msaa_samples = 4;
};
```

### 2.2. Swap Chain

```cpp
class SwapChain {
public:
    // Get current frame texture
    Texture* current_texture();
    
    // Resize
    void resize(uint32_t width, uint32_t height);
    
    // Present
    void present();
    
    // Info
    uint32_t width() const;
    uint32_t height() const;
    float dpi_scale() const;       // 1.0 = 96 DPI, 2.0 = Retina
    
    // VSync
    void set_vsync(bool enabled);
    bool vsync_enabled() const;
    
    // Format
    TextureFormat format() const;  // Typically RGBA8 or BGRA8
};
```

### 2.3. GPU Resource Lifecycle

```cpp
// Resource ownership and lifecycle:
//
// Buffer / Texture / Pipeline
//   → Created by GPUDevice
//   → Owned by the creating system
//   → Destroyed when the system shuts down
//   → GPU memory is tracked per-resource
//
// Temporary resources (per-frame):
//   → Allocated from a pool at frame start
//   → Freed at frame end
//   → Reused to avoid allocation overhead

class GPUMemoryManager {
    uint64_t total_allocated_bytes() const;
    uint64_t used_bytes() const;
    uint64_t available_bytes() const;
    
    struct Budget {
        uint64_t budget_bytes;
        uint64_t usage_bytes;
        float usage_percent;
    };
    Budget current_budget() const;
    
    void report_memory_pressure();  // Free unused resources
};
```

---

## 3. Skia Integration

### 3.1. Skia Canvas

```cpp
class SkiaCanvas {
public:
    SkiaCanvas(GPUDevice* device);
    ~SkiaCanvas();
    
    // Begin/end frame
    void begin_frame(SwapChain* swapchain);
    void end_frame();
    
    // Drawing operations
    void draw_rect(const Rect& rect, const Paint& paint);
    void draw_rounded_rect(const RoundedRect& rect, const Paint& paint);
    void draw_circle(const Point& center, float radius, const Paint& paint);
    void draw_line(const Point& p1, const Point& p2, const Paint& paint);
    void draw_path(const Path& path, const Paint& paint);
    void draw_text(const std::string& text, const Point& pos,
                   const Font& font, const Paint& paint);
    void draw_image(const Image* image, const Rect& dest,
                    const Rect* src = nullptr);
    void draw_svg(const SVG* svg, const Rect& dest);
    void draw_shadow(const RoundedRect& rect, float blur_radius,
                     const Color& color, const Point& offset);
    
    // Clipping
    void clip_rect(const Rect& rect);
    void clip_rounded_rect(const RoundedRect& rect);
    void clip_path(const Path& path);
    void reset_clip();
    
    // State
    void save();
    void restore();
    void translate(float x, float y);
    void scale(float sx, float sy);
    void rotate(float degrees);
    void set_alpha(float alpha);
    void set_matrix(const Matrix& matrix);
    
    // GPU surface
    SkSurface* gpu_surface() const;

private:
    sk_sp<SkSurface> surface_;
    SkCanvas* canvas_;
    SkiaState state_;
};
```

### 3.2. Skia vs Native WebGPU

```cpp
// Hybrid rendering strategy:
//
// 95% of UI: Skia (high-level 2D)
//   - Widgets, text, icons, buttons, panels
//   - Waveforms, piano roll, automation curves
//   - Shadows, blur effects
//   - SVG icons
//   - Text rendering with font shaping
//
// 5% custom WebGPU: Special effects
//   - Spectrum analyzer shaders
//   - Convolution reverb visualization
//   - Custom particle effects
//   - Video rendering (future)
//   - High-performance waveform rendering (10M+ samples)
//
// Both render to the same GPU texture via
// interop between Skia and WebGPU

class HybridRenderer {
    // Create a shared texture for interop
    Texture* create_shared_texture(uint32_t width, uint32_t height);
    
    // Render custom WebGPU content to shared texture
    void render_custom(CommandBuffer* cmd, Texture* target);
    
    // Skia draws over the shared texture
    void render_skia(SkiaCanvas* canvas, Texture* shared);
};
```

### 3.3. Font Rendering

```cpp
class FontManager {
public:
    // Register fonts
    void register_font(const std::string& path);
    void register_font_memory(const void* data, size_t size);
    
    // Load system fonts
    void load_system_fonts();
    
    // Get font
    SkFont get_font(const std::string& family, float size,
                    SkFontStyle::Weight weight);
    
    // Font fallback chain
    void set_fallback_fonts(const std::vector<std::string>& families);
    
    // Character coverage
    bool has_glyph(const std::string& family, uint32_t codepoint);
    
    // Font cache
    struct FontCacheStats {
        size_t cache_size_bytes;
        uint32_t glyph_count;
        float cache_hit_rate;
    };
    FontCacheStats stats() const;

private:
    sk_sp<SkTypeface> load_typeface(const std::string& path);
    std::unordered_map<std::string, sk_sp<SkTypeface>> typefaces_;
};
```

---

## 4. Render Pipeline

### 4.1. Frame Pipeline

```
Start Frame
    │
    ├──► Acquire next swapchain image
    │
    ├──► Begin GPU command buffer
    │
    ├──► Render Pass: Opaque UI
    │   ├── Clear background
    │   ├── Draw solid widgets (buttons, panels, tracks)
    │   └── Draw text (labels, values, names)
    │
    ├──► Render Pass: Note Grid (Piano Roll)
    │   ├── Draw grid lines
    │   ├── Draw note bodies (instanced quads)
    │   ├── Draw note velocity overlays
    │   └── Draw selection outlines
    │
    ├──► Render Pass: Waveforms
    │   ├── Upload waveform vertex data
    │   └── Draw waveform paths
    │
    ├──► Render Pass: Automation Curves
    │   ├── Generate curve tessellation
    │   └── Draw line strips
    │
    ├──► Render Pass: Overlays
    │   ├── Draw drag-and-drop indicators
    │   ├── Draw tooltips
    │   ├── Draw context menus
    │   └── Draw selection rectangles
    │
    ├──► Render Pass: Post-Processing
    │   ├── Apply blur (for modal backgrounds)
    │   ├── Apply glow effects
    │   └── Tone mapping
    │
    ├──► Submit command buffer
    │
    └──► Present swapchain
```

### 4.2. Render Graph

```cpp
class RenderGraph {
public:
    // Define render passes and their dependencies
    struct PassDesc {
        std::string name;
        std::vector<Texture*> color_attachments;
        Texture* depth_stencil = nullptr;
        std::vector<Texture*> inputs;     // Read-only inputs
        std::vector<ClearValue> clears;
        RenderPassFlags flags;
    };
    
    PassID add_pass(const PassDesc& desc);
    void add_dependency(PassID from, PassID to);
    
    // Build and execute
    void compile();           // Optimize pass ordering, merging
    void execute(CommandBuffer* cmd);
    
    // Resource management
    Texture* get_pass_output(PassID id, uint32_t attachment = 0);

private:
    std::vector<RenderPass> passes_;
    std::vector<PassDependency> dependencies_;
    
    // Auto-barrier insertion
    void insert_barriers(CommandBuffer* cmd,
                         PassID from, PassID to);
};
```

### 4.3. Command Buffer

```cpp
class CommandBuffer {
public:
    void begin();
    void end();
    
    // Render pass
    void begin_render_pass(const RenderPassDesc& desc);
    void end_render_pass();
    
    // Draw calls
    void draw(uint32_t vertex_count, uint32_t instance_count = 1);
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1);
    void draw_indirect(Buffer* indirect_buffer);
    
    // Compute (for post-processing)
    void begin_compute_pass();
    void end_compute_pass();
    void dispatch(uint32_t x, uint32_t y, uint32_t z);
    
    // Resource binding
    void set_pipeline(RenderPipeline* pipeline);
    void set_bind_group(uint32_t group, BindGroup* bindings);
    void set_vertex_buffer(uint32_t slot, Buffer* buffer);
    void set_index_buffer(Buffer* buffer, IndexFormat format);
    void set_push_constants(const void* data, uint32_t size);
    
    // Resource barriers
    void buffer_barrier(Buffer* buffer, ResourceState from, ResourceState to);
    void texture_barrier(Texture* texture, ResourceState from, ResourceState to);
};
```

---

## 5. Shaders

### 5.1. Shader Types

```cpp
// Shader language: WGSL (WebGPU Shading Language)
//
// Built-in shaders:

// 1. Note rendering (instanced):
//   - Vertex: Transforms note position/size from instance data
//   - Fragment: Applies note color, velocity gradient, selection outline

// 2. Waveform rendering:
//   - Vertex: Generates waveform polyline
//   - Fragment: Solid color with alpha gradient at edges

// 3. Automation curves:
//   - Vertex: Tessellates Bezier curves into line segments
//   - Fragment: Solid color with optional glow

// 4. Grid lines:
//   - Vertex: Simple line vertices
//   - Fragment: Single color, alpha based on zoom level

// 5. Text:
//   - Vertex: Glyph quad from glyph atlas
//   - Fragment: Texture sampling from glyph atlas, SDF rendering

// 6. Blur (post-process):
//   - Compute: Separated Gaussian blur (horizontal + vertical passes)
//   - Parameters: blur_radius, direction

// 7. Spectrum analyzer:
//   - Compute: FFT magnitude to bar positions
//   - Vertex: Generates bar quads
//   - Fragment: Gradient from cold to hot
```

### 5.2. Example: Note Shader

```wgsl
// Note instance vertex shader
struct NoteInstance {
    position_x: f32,
    position_y: f32,
    width: f32,
    height: f32,
    color: vec4<f32>,
    velocity: f32,
    flags: u32,    // selected, muted, ghost
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec4<f32>,
    @location(2) velocity: f32,
    @location(3) flags: u32,
};

@vertex
fn vs_main(@builtin(vertex_index) vertex_id: u32,
           @builtin(instance_index) instance_id: u32,
           @binding(0) instances: array<NoteInstance>)
           -> VertexOutput {
    let note = instances[instance_id];
    
    // Quad vertices: 0=TL, 1=TR, 2=BL, 3=BR
    let quad_pos = array<vec2<f32>, 4>(
        vec2<f32>(0.0, 0.0),
        vec2<f32>(1.0, 0.0),
        vec2<f32>(0.0, 1.0),
        vec2<f32>(1.0, 1.0)
    );
    
    let uv = quad_pos[vertex_id];
    let world_pos = vec2<f32>(
        note.position_x + uv.x * note.width,
        note.position_y + uv.y * note.height
    );
    
    var output: VertexOutput;
    output.position = vec4<f32>(world_pos, 0.0, 1.0);
    output.uv = uv;
    output.color = note.color;
    output.velocity = note.velocity;
    output.flags = note.flags;
    return output;
}

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    var color = input.color;
    
    // Velocity gradient (bottom-up)
    let velocity_strength = smoothstep(0.0, 1.0, input.velocity);
    color.a *= max(0.15, velocity_strength);
    
    // Selection outline
    if (input.flags & 0x1u) != 0u {
        // White outline on edge pixels
        let edge = 1.0 - 4.0 * min(input.uv.x, 1.0 - input.uv.x,
                                    input.uv.y, 1.0 - input.uv.y);
        if edge > 0.0 {
            color = vec4<f32>(1.0, 1.0, 1.0, edge * 0.8);
        }
    }
    
    // Muted overlay
    if (input.flags & 0x2u) != 0u {
        color.rgb = mix(color.rgb, vec3<f32>(0.3), 0.5);
    }
    
    return color;
}
```

---

## 6. Texture Management

### 6.1. Texture Manager

```cpp
class TextureManager {
public:
    // Load texture from file
    Texture* load_from_file(const std::string& path);
    
    // Load texture from memory
    Texture* load_from_memory(const void* data, size_t size);
    
    // Create texture
    Texture* create(const TextureDesc& desc);
    
    // Create atlas (pack multiple images into one texture)
    TextureAtlas* create_atlas(const std::vector<ImageDesc>& images);
    
    // Destroy
    void destroy(Texture* texture);
    
    // Cache
    void set_cache_size(uint64_t max_bytes);
    uint64_t cache_usage() const;
    void purge_cache();

private:
    std::unordered_map<std::string, Texture*> cache_;
    std::vector<Texture*> textures_;
};
```

### 6.2. Glyph Atlas

```cpp
// Text rendering uses a glyph atlas:
// 1. All frequently used glyphs are rendered once
// 2. Packed into a single large texture
// 3. Text rendering samples from the atlas
// 4. New glyphs are added on demand

class GlyphAtlas {
public:
    struct GlyphInfo {
        uint32_t atlas_x, atlas_y;
        uint32_t width, height;
        float u0, v0, u1, v1;  // UV coordinates
        float advance;
        float bearing_x, bearing_y;
    };
    
    const GlyphInfo* get_glyph(const std::string& font_family,
                                uint32_t codepoint, float size);
    
    Texture* atlas_texture() const;
    
    // Atlas is typically 4096x4096 (16MB at RGBA8)
    static constexpr uint32_t ATLAS_SIZE = 4096;
    static constexpr uint32_t MAX_GLYPH_SIZE = 256;
};
```

### 6.3. Icon Atlas

```cpp
// Icons are packed into an atlas at startup:
// - All SVG icons are rendered once
// - Packed into a 2048x2048 texture
// - Rendered as textured quads
// - Color is applied via shader (multiply)
//
// Icon atlas only needs to be rebuilt when
// themes change (different icon set).
```

---

## 7. Post-Processing

### 7.1. Effect Pipeline

```cpp
class PostProcessor {
public:
    // Blur (for modal overlays, shadows)
    void blur(Texture* input, Texture* output, float radius);
    
    // Glow effect (for selected items, accent highlights)
    void glow(Texture* input, Texture* output,
              const Color& color, float radius, float intensity);
    
    // Drop shadow
    void drop_shadow(Texture* input, Texture* output,
                     float radius, float offset_x, float offset_y,
                     const Color& color);
    
    // Tone mapping (HDR display support)
    void tone_map(Texture* input, Texture* output);
    
    // Color blindness simulation (accessibility)
    enum class Simulation { Protanopia, Deuteranopia, Tritanopia };
    void simulate_color_blindness(Texture* input, Texture* output,
                                   Simulation type);
    
    // Gamma correction
    void apply_gamma(Texture* input, Texture* output, float gamma);
};
```

### 7.2. Blur Performance

```
// Gaussian blur performance (1080p, RTX 3060):
//
// Radius  | 2-pass (H+V) |  Total
// ───────────────────────────────
// 4px     | 0.1ms         | 0.2ms
// 8px     | 0.15ms        | 0.3ms
// 16px    | 0.2ms         | 0.4ms
// 32px    | 0.3ms         | 0.6ms
// 64px    | 0.5ms         | 1.0ms
//
// All blur is done via compute shaders
// (separable Gaussian, linear sampling optimization)
```

---

## 8. HiDPI Support

### 8.1. HiDPI Handling

```cpp
class HiDPIManager {
public:
    // Get display scale factor
    float scale_factor(void* monitor_handle);
    
    // Convert logical to physical pixels
    uint32_t logical_to_physical(float logical_pixels,
                                  float scale) const;
    float physical_to_logical(uint32_t physical_pixels,
                              float scale) const;
    
    // Render at native resolution
    // On Retina (2x): physical = 2 * logical
    // On 150% scaling: physical = 1.5 * logical
    //
    // All Skia rendering is done at physical pixel resolution
    // Text renders at native retina resolution (crisp)
    // Bitmaps are upscaled if needed
    
    // UI scale override (accessibility)
    void set_ui_scale(float scale);  // 0.75x to 2.0x
    float ui_scale() const;
};
```

---

## 9. Multi-Window Rendering

### 9.1. Multi-Window Architecture

```cpp
class MultiWindowRenderer {
public:
    // Create a new rendering context for a window
    WindowContext* create_window(void* native_handle,
                                  uint32_t width, uint32_t height);
    
    // Destroy window context
    void destroy_window(WindowContext* ctx);
    
    // Render a frame for a specific window
    void render_frame(WindowContext* ctx);
    
    // Each window has its own:
    // - Swap chain
    // - Command buffer
    // - Render targets
    // - Skia canvas
    //
    // Shared resources:
    // - GPU device
    // - Texture atlases (glyphs, icons)
    // - Shader modules
    // - Buffer pools
    
    struct WindowContext {
        SwapChain* swapchain;
        SkiaCanvas* canvas;
        CommandBuffer* cmd;
        uint32_t width, height;
        float dpi_scale;
    };
};
```

---

## 10. Performance

### 10.1. Performance Targets

| Resolution | Target FPS | GPU Required |
|---|---|---|
| 1920×1080 | 144 FPS | GTX 1660 / RX 580 |
| 2560×1440 | 120 FPS | RTX 2060 / RX 6600 |
| 3840×2160 (4K) | 60 FPS | RTX 3070 / RX 6800 |
| 5120×2880 (5K) | 60 FPS | RTX 3080+ |

### 10.2. Frame Time Budget

```
At 144 FPS (6.94ms per frame):
  Skia draw commands:   2.0ms
  GPU rendering:        2.0ms
  Post-processing:      1.0ms
  Present:              0.5ms
  Driver overhead:      1.0ms
  Buffer:               0.44ms
  ─────────────────────────
  Total:                6.94ms

At 60 FPS (16.67ms per frame):
  Skia draw commands:   5.0ms
  GPU rendering:        4.0ms
  Post-processing:      2.0ms
  Present:              0.5ms
  Driver overhead:      2.0ms
  Buffer:               3.17ms
```

### 10.3. Draw Call Budget

```cpp
// ARDA targets < 1000 draw calls per frame at 144 FPS:
//
// UI widgets:      ~200 draw calls (batched by Skia)
// Piano roll:      ~50 draw calls (instanced, batched)
// Waveforms:       ~30 draw calls (per track)
// Automation:      ~20 draw calls
// Text:            ~100 draw calls (glyph runs)
// Post-processing: ~10 draw calls
// ────────────────────────────────────
// Total:           ~410 draw calls
```

### 10.4. Optimization Strategies

```cpp
// 1. Instanced rendering
//    - All piano roll notes in ONE draw call
//    - All grid lines in ONE draw call
//    - Instance data: position, size, color, flags

// 2. Texture atlasing
//    - All icons in one texture
//    - All glyphs in one texture
//    - Reduces texture binds per frame

// 3. Buffer pooling
//    - Reuse vertex/index buffers across frames
//    - Use persistent mapping + WRITE_NO_OVERWRITE

// 4. Dirty rect tracking
//    - Only redraw regions that changed
//    - Skia's SkCanvas::quickReject for culling

// 5. LOD for distant elements
//    - Zoomed-out piano roll: simplified notes
//    - Waveforms at low zoom: peak-only display

// 6. GPU-driven rendering
//    - Compute culling on GPU
//    - Indirect draw commands from compute shaders
```

---

## 11. API Reference

### 11.1. Public API

```cpp
class RenderEngine {
public:
    // Initialize
    bool init(const RenderConfig& config);
    void shutdown();
    
    // Device
    GPUDevice* device();
    
    // Canvas
    SkiaCanvas* main_canvas();
    
    // Windows
    MultiWindowRenderer& windows();
    
    // Post-processing
    PostProcessor& post();
    
    // Fonts
    FontManager& fonts();
    
    // Textures
    TextureManager& textures();
    
    // HiDPI
    HiDPIManager& hidpi();
    
    // Performance
    struct FrameStats {
        float frame_time_ms;
        float cpu_time_ms;
        float gpu_time_ms;
        uint32_t draw_calls;
        uint32_t triangles;
        uint64_t gpu_memory_bytes;
    };
    FrameStats last_frame_stats() const;
    
    // VSync
    void set_vsync(bool enabled);
    bool vsync_enabled() const;
    
    // Config
    RenderConfig config() const;
};
```

---

## 12. RFC Index

| RFC | Component | Status |
|---|---|---|
| RFC-REN-001 | WebGPU Device & Swap Chain | 🔲 Pending |
| RFC-REN-002 | Skia Integration & Canvas | 🔲 Pending |
| RFC-REN-003 | Hybrid Rendering (Skia + Custom WebGPU) | 🔲 Pending |
| RFC-REN-004 | Render Graph & Pass Management | 🔲 Pending |
| RFC-REN-005 | Note Shader (Instanced) | 🔲 Pending |
| RFC-REN-006 | Waveform Shader | 🔲 Pending |
| RFC-REN-007 | Automation Curve Tessellation | 🔲 Pending |
| RFC-REN-008 | Glyph Atlas & Text Rendering | 🔲 Pending |
| RFC-REN-009 | Icon Atlas | 🔲 Pending |
| RFC-REN-010 | Gaussian Blur Compute Shader | 🔲 Pending |
| RFC-REN-011 | Glow & Shadow Post-Processing | 🔲 Pending |
| RFC-REN-012 | HiDPI & UI Scaling | 🔲 Pending |
| RFC-REN-013 | Multi-Window Rendering | 🔲 Pending |
| RFC-REN-014 | GPU Memory Management | 🔲 Pending |
| RFC-REN-015 | Draw Call Batching | 🔲 Pending |
| RFC-REN-016 | Dirty Rect Tracking | 🔲 Pending |
| RFC-REN-017 | LOD for Zoom Levels | 🔲 Pending |
| RFC-REN-018 | Frame Timing & Profiling | 🔲 Pending |

---

## Appendix A: Supported GPUs

| Vendor | Min GPU | Min Driver |
|---|---|---|
| NVIDIA | GTX 960 | Vulkan 1.2 |
| AMD | RX 470 | Vulkan 1.2 |
| Intel | UHD 630 | Vulkan 1.2 |
| Apple | M1 | Metal 3 |
| Software | — | SwiftShader (fallback) |

## Appendix B: GPU Memory Budget

| Resource | Budget |
|---|---|
| Frame buffers (double-buffered, 4K) | 64 MB |
| Glyph atlas (4096×4096) | 16 MB |
| Icon atlas (2048×2048) | 4 MB |
| Waveform vertex buffers (100 tracks) | 16 MB |
| Note instance buffers (100k notes) | 8 MB |
| Skia GPU cache | 64 MB |
| Texture cache | 128 MB |
| Post-processing targets | 32 MB |
| **Total** | **~332 MB** |
