#include "graphics/render_loop.h"
#include "graphics/focus_manager.h"
#include "graphics/graphics_engine.h"
#include "graphics/shortcut_manager.h"
#include "graphics/skia_canvas.h"
#include "graphics/widget.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>

namespace aria {

// =====================================================================
// Lifecycle
// =====================================================================

RenderLoop::RenderLoop() {
    frame_start_ = Clock::now();
    last_present_time_ = Clock::now();
}

RenderLoop::~RenderLoop() {
    stop();
}

void RenderLoop::init(GraphicsEngine* engine, SkiaCanvas* canvas, Widget* root) {
    engine_ = engine;
    canvas_ = canvas;
    root_ = root;
}

void RenderLoop::start() {
    running_ = true;
    frame_start_ = Clock::now();
    last_present_time_ = Clock::now();
    over_budget_count_ = 0;
    budget_warning_ = false;
    counters_ = {};
    histogram_index_ = 0;
    histogram_count_ = 0;
    frame_time_history_.fill(0.0f);
}

void RenderLoop::stop() {
    running_ = false;
}

bool RenderLoop::set_target_fps(int fps) {
    if (fps < 1 || fps > 144) return false;
    target_fps_ = fps;
    frame_budget_ms_ = 1000.0f / static_cast<float>(fps);
    return true;
}

float RenderLoop::frame_budget_ms() const noexcept {
    return frame_budget_ms_;
}

// =====================================================================
// Frame Tick
// =====================================================================

bool RenderLoop::tick() {
    if (!running_ || !canvas_) return false;

    // 1. Measure frame start
    TimePoint tick_start = Clock::now();
    auto frame_elapsed = std::chrono::duration<float, std::milli>(
        tick_start - frame_start_).count();
    frame_start_ = tick_start;

    // 2. Layout pass (if root is available)
    if (root_) {
        // Measure + arrange done here; full LayoutEngine integration
        // would be called from the application layer.
    }

    // 3. Animation tick (if animator is available)
    if (animator_) {
        animator_->tick(frame_elapsed);

        // Mark completed widgets dirty so they get re-painted
        for (auto id : animator_->completed()) {
            if (root_) {
                auto* widget = root_->find_by_id(id);
                if (widget) {
                    widget->mark_dirty();
                }
            }
        }
    }

    // 4. Paint pass
    if (root_) {
        root_->paint(canvas_);
    }

    // 4b. Render focus ring (after paint, before flush)
    if (focus_manager_) {
        focus_manager_->render_focus_ring(canvas_, root_);
    }

    // 5. Flush Skia commands
    TimePoint flush_start = Clock::now();
    canvas_->flush();
    auto flush_elapsed = std::chrono::duration<float, std::milli>(
        Clock::now() - flush_start).count();

    // 6. Present to swap chain
    TimePoint present_start = Clock::now();
    if (engine_) {
        // Present the current frame. If no swap chain has been created
        // (e.g. in headless mode), this is a safe no-op.
        engine_->present(nullptr);
    }
    auto present_elapsed = std::chrono::duration<float, std::milli>(
        Clock::now() - present_start).count();

    // 7. Update profiling counters
    uint32_t draw_calls = canvas_->draw_call_count();
    update_counters(frame_elapsed, flush_elapsed,
                    present_elapsed, draw_calls);

    // 7b. PR 4: Record frame time into histogram
    record_frame_time(frame_elapsed);

    // 8. Frame pacing
    float frame_time = frame_elapsed;
    check_budget(frame_time);

    float sleep_time = frame_budget_ms_ - frame_time;
    if (sleep_time > 0.0f) {
        // Sleep for the remaining budget to maintain precise pacing
        auto sleep_duration = std::chrono::duration<float, std::milli>(sleep_time);
        std::this_thread::sleep_for(sleep_duration);
    }

    last_present_time_ = Clock::now();
    return running_;
}

// =====================================================================
// Internal
// =====================================================================

void RenderLoop::update_counters(float frame_time_ms, float flush_ms,
                                  float present_ms, uint32_t draw_calls) {
    counters_.frame_time_ms = frame_time_ms;
    counters_.flush_time_ms = flush_ms;
    counters_.present_time_ms = present_ms;
    counters_.draw_call_count = draw_calls;

    // Compute measured FPS from frame time
    if (frame_time_ms > 0.0f) {
        counters_.fps = static_cast<int>(1000.0f / frame_time_ms);
    } else {
        counters_.fps = target_fps_;
    }
}

void RenderLoop::check_budget(float frame_time_ms) {
    constexpr float kWarningThreshold = 0.80f;  // 80% of budget
    constexpr int kWarningFrames = 10;
    constexpr int kRateDropFrames = 30;

    if (frame_time_ms > frame_budget_ms_) {
        ++over_budget_count_;
        ++consecutive_warning_frames_;
    } else if (frame_time_ms > frame_budget_ms_ * kWarningThreshold) {
        // Over 80% — accumulate warning counter
        ++consecutive_warning_frames_;
        // Not fully over budget, so don't increment over_budget_count_
    } else {
        // Well under budget — reset warning counters
        consecutive_warning_frames_ = 0;
    }

    // Emit warning at 10 consecutive warning frames
    if (consecutive_warning_frames_ >= kWarningFrames) {
        budget_warning_ = true;
    } else {
        budget_warning_ = false;
    }

    // Drop to 60 FPS after 30 consecutive over-budget frames
    if (over_budget_count_ >= kRateDropFrames) {
        if (target_fps_ != 60) {
            set_target_fps(60);
        }
    }
}

FrameCounters RenderLoop::profiling_counters() const noexcept {
    return counters_;
}

// =====================================================================
// PR 4: Profiling histogram
// =====================================================================

void RenderLoop::record_frame_time(float frame_time_ms) {
    // Store in circular buffer
    frame_time_history_[histogram_index_] = frame_time_ms;
    histogram_index_ = (histogram_index_ + 1) % kHistogramWindow;
    if (histogram_count_ < kHistogramWindow) {
        ++histogram_count_;
    }

    // Increment total frame count
    ++counters_.frame_count;

    // Check budget violation
    if (frame_time_ms > frame_budget_ms_) {
        ++counters_.budget_violations;
    }

    // Recompute histogram stats
    recompute_histogram();
}

void RenderLoop::recompute_histogram() {
    if (histogram_count_ == 0) {
        counters_.frame_time_min = 0.0f;
        counters_.frame_time_max = 0.0f;
        counters_.frame_time_avg = 0.0f;
        return;
    }

    float sum = 0.0f;
    float min_val = std::numeric_limits<float>::max();
    float max_val = 0.0f;

    size_t count = histogram_count_;
    for (size_t i = 0; i < count; ++i) {
        float t = frame_time_history_[i];
        sum += t;
        if (t < min_val) min_val = t;
        if (t > max_val) max_val = t;
    }

    counters_.frame_time_min = min_val;
    counters_.frame_time_max = max_val;
    counters_.frame_time_avg = sum / static_cast<float>(count);
}

// =====================================================================
// Input system integration
// =====================================================================

bool RenderLoop::process_key_chord(int key_code, bool ctrl, bool shift,
                                    bool alt, bool meta) {
    // Tab key → FocusManager navigation
    if (key_code == 9 && focus_manager_) {  // Tab key code
        if (shift) {
            focus_manager_->focus_previous();
        } else {
            focus_manager_->focus_next();
        }
        return true;
    }

    // ShortcutManager dispatch
    if (shortcut_manager_) {
        KeyChord chord;
        chord.key   = key_code;
        chord.ctrl  = ctrl;
        chord.shift = shift;
        chord.alt   = alt;
        chord.meta  = meta;
        return shortcut_manager_->dispatch(chord);
    }

    return false;
}

} // namespace aria
