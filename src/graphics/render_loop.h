#ifndef ARIA_RENDER_LOOP_H
#define ARIA_RENDER_LOOP_H

#include "graphics/graphics_types.h"

#include <array>
#include <chrono>
#include <cstdint>

namespace aria {

// ── Forward declarations ─────────────────────────────────────────

class Animator;
class FocusManager;
class ShortcutManager;

class GraphicsEngine;
class SkiaCanvas;
class Widget;

// ── FrameCounters ────────────────────────────────────────────────

/// Per-frame profiling data exposed by RenderLoop.
struct FrameCounters {
    float frame_time_ms    = 0.0f;  ///< Total frame time (ms)
    float flush_time_ms    = 0.0f;  ///< Skia flush duration (ms)
    float present_time_ms  = 0.0f;  ///< Dawn present duration (ms)
    uint32_t draw_call_count = 0;   ///< Draw calls this frame
    int      fps           = 0;     ///< Current measured FPS

    // ── PR 4: Profiling histogram ──────────────────────────────
    float   frame_time_min  = 0.0f;  ///< Minimum frame time over sliding window
    float   frame_time_max  = 0.0f;  ///< Maximum frame time over sliding window
    float   frame_time_avg  = 0.0f;  ///< Average frame time over sliding window
    uint64_t frame_count    = 0;     ///< Total frames recorded
    uint32_t budget_violations = 0;  ///< Frames exceeding frame budget
};

// ── RenderLoop ───────────────────────────────────────────────────

/// Drives the GPU rendering frame loop at a target FPS.
///
/// Owned by Application::run(), the RenderLoop manages frame timing,
/// vsync-aligned present, and profiling counter collection.
///
/// Design decisions (from design.md):
///   - Targets 144 FPS (6.94 ms budget).
///   - Uses steady_clock for high-resolution timing.
///   - Sleeps when frame completes under budget; presents immediately
///     when over budget.
///   - Emits kFrameBudgetExceeded when over budget.
///   - Drops to 60 FPS after 30 consecutive over-budget frames.
class RenderLoop {
public:
    RenderLoop();
    ~RenderLoop();

    RenderLoop(const RenderLoop&) = delete;
    RenderLoop& operator=(const RenderLoop&) = delete;
    RenderLoop(RenderLoop&&) = delete;
    RenderLoop& operator=(RenderLoop&&) = delete;

    // ── Lifecycle ───────────────────────────────────────────────

    /// Initialise the render loop with a graphics engine and root widget.
    void init(GraphicsEngine* engine, SkiaCanvas* canvas, Widget* root);

    /// Tick one frame. Returns true if the loop should continue running.
    bool tick();

    /// Start the loop.
    void start();

    /// Stop the loop.
    void stop();

    bool is_running() const noexcept { return running_; }

    // ── FPS configuration ───────────────────────────────────────

    int target_fps() const noexcept { return target_fps_; }

    /// Set the target FPS. Clamped to [1, 144]. Returns true if changed.
    bool set_target_fps(int fps);

    /// Frame budget in milliseconds for the current target FPS.
    float frame_budget_ms() const noexcept;

    // ── Profiling ───────────────────────────────────────────────

    /// Snapshot of counters from the most recent completed frame.
    FrameCounters profiling_counters() const noexcept;

    // ── Animation integration ──────────────────────────────────

    /// Set the Animator instance to tick each frame.
    /// Pass nullptr to clear (animations are skipped when null).
    void set_animator(Animator* anim) noexcept { animator_ = anim; }

    // ── Input system integration ────────────────────────────────

    /// Set the FocusManager for tab-order navigation and focus ring.
    /// Pass nullptr to disable focus management.
    void set_focus_manager(FocusManager* fm) noexcept { focus_manager_ = fm; }

    /// Set the ShortcutManager for keyboard shortcut dispatch.
    /// Pass nullptr to disable shortcut processing.
    void set_shortcut_manager(ShortcutManager* sm) noexcept { shortcut_manager_ = sm; }

    /// Process a keyboard chord through the input system.
    /// Dispatches via ShortcutManager and handles Tab for focus
    /// navigation. Returns true if the chord was consumed.
    bool process_key_chord(int key_code, bool ctrl, bool shift,
                           bool alt, bool meta);

    // ── Budget monitoring ───────────────────────────────────────

    bool is_budget_warning() const noexcept { return budget_warning_; }
    int consecutive_over_budget() const noexcept { return over_budget_count_; }

    // ── PR 4: Profiling histogram ──────────────────────────────

    /// Record a frame time into the histogram. Recomputes min/max/avg
    /// and increments budget_violations if frame_time exceeds budget.
    void record_frame_time(float frame_time_ms);

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // Configuration
    int target_fps_ = 144;
    float frame_budget_ms_ = 1000.0f / 144.0f;

    // State
    bool running_ = false;
    bool budget_warning_ = false;
    int over_budget_count_ = 0;
    int consecutive_warning_frames_ = 0;

    // Timing
    TimePoint frame_start_;
    TimePoint last_present_time_;

    // Profiling
    FrameCounters counters_;

    // ── PR 4: Frame time histogram ─────────────────────────────
    static constexpr size_t kHistogramWindow = 120;  ///< ~1s at 144 FPS
    std::array<float, kHistogramWindow> frame_time_history_{};
    size_t histogram_index_ = 0;
    size_t histogram_count_ = 0;

    // Dependencies (non-owning)
    GraphicsEngine* engine_ = nullptr;
    SkiaCanvas* canvas_ = nullptr;
    Widget* root_ = nullptr;
    Animator* animator_ = nullptr;
    FocusManager* focus_manager_ = nullptr;
    ShortcutManager* shortcut_manager_ = nullptr;

    void update_counters(float frame_time_ms, float flush_ms,
                         float present_ms, uint32_t draw_calls);
    void check_budget(float frame_time_ms);
    void recompute_histogram();
};

} // namespace aria

#endif // ARIA_RENDER_LOOP_H
