#include "velocity_lane.h"
#include "piano_roll_canvas.h"  // For ViewTransform

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace aria {

// ═════════════════════════════════════════════════════════════════
// Colour helper
// ═════════════════════════════════════════════════════════════════

Rgba VelocityLane::velocity_color(uint8_t velocity) {
    // Three-stop gradient: blue (low) → green (mid) → red (high)
    constexpr Rgba low{0.290f, 0.620f, 1.0f, 1.0f};    // #4A9EFF
    constexpr Rgba mid{0.478f, 1.0f,  0.478f, 1.0f};    // #7AFF7A
    constexpr Rgba high{1.0f, 0.478f, 0.290f, 1.0f};    // #FF7A4A

    float t = static_cast<float>(velocity) / 127.0f;
    if (t <= 0.5f) {
        float u = t / 0.5f;
        return Rgba{
            low.r + (mid.r - low.r) * u,
            low.g + (mid.g - low.g) * u,
            low.b + (mid.b - low.b) * u,
            1.0f
        };
    } else {
        float u = (t - 0.5f) / 0.5f;
        return Rgba{
            mid.r + (high.r - mid.r) * u,
            mid.g + (high.g - mid.g) * u,
            mid.b + (high.b - mid.b) * u,
            1.0f
        };
    }
}

// ═════════════════════════════════════════════════════════════════
// Curve factor helper
// ═════════════════════════════════════════════════════════════════

double VelocityLane::curve_factor(double t, CurveType type) {
    t = std::clamp(t, 0.0, 1.0);
    switch (type) {
    case CurveType::Linear:
        return t;
    case CurveType::Exponential:
        return t * t;
    case CurveType::Logarithmic:
        return std::sqrt(t);  // Inverse of exponential
    case CurveType::SCurve:
        return t * t * (3.0 - 2.0 * t);  // Smoothstep
    default:
        return t;
    }
}

// ═════════════════════════════════════════════════════════════════
// Rendering
// ═════════════════════════════════════════════════════════════════

void VelocityLane::render(SkiaCanvas* /*canvas*/, const Rect& bounds,
                           const ViewTransform& view) {
    if (!notes_) return;

    // ─── Render pipeline (stubbed — SkiaCanvas is opaque) ─────────
    // For each visible note, draw a velocity bar:
    //
    // for (const auto& n : notes_->notes()) {
    //     // Cull invisible notes.
    //     double x = view.ppqn_to_x(n.start_ppqn);
    //     double w = n.duration_ppqn / view.ppqn_per_pixel_x;
    //     if (x + w < bounds.x || x > bounds.x + bounds.width) continue;
    //
    //     // Bar dimensions.
    //     double bar_height = (n.velocity / 127.0) * bounds.height;
    //     double bar_y = bounds.y + bounds.height - bar_height;
    //
    //     // Colour by velocity.
    //     Rgba color = velocity_color(n.velocity);
    //
    //     // Draw bar.
    //     canvas->drawRect(x, bar_y, w, bar_height, color);
    //
    //     // Selected note outline (white border).
    //     if (notes_->selected().count(n.id)) {
    //         canvas->drawRectOutline(x, bar_y, w, bar_height, 1.0f, WHITE);
    //     }
    // }
    //
    // ─── Background fill ─────────────────────────────────────────
    //   canvas->drawRect(bounds.x, bounds.y, bounds.width, bounds.height, bg);

    (void)bounds;
    (void)view;
}

// ═════════════════════════════════════════════════════════════════
// Individual editing
// ═════════════════════════════════════════════════════════════════

void VelocityLane::set_velocity(NoteID id, uint8_t velocity) {
    if (!notes_) return;
    Note* note = notes_->find(id);
    if (note) {
        note->velocity = std::min<uint8_t>(velocity, 127);
    }
}

// ═════════════════════════════════════════════════════════════════
// Bulk editing
// ═════════════════════════════════════════════════════════════════

void VelocityLane::ramp_selection(const std::vector<Note*>& notes,
                                   uint8_t start_vel, uint8_t end_vel) {
    if (notes.empty()) return;

    // Sort notes by their start position so the ramp is from left to right.
    std::vector<Note*> sorted = notes;
    std::sort(sorted.begin(), sorted.end(),
              [](const Note* a, const Note* b) {
                  return a->start_ppqn < b->start_ppqn;
              });

    size_t count = sorted.size();
    if (count == 1) {
        sorted[0]->velocity = start_vel;
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(count - 1);
        double vel = static_cast<double>(start_vel) +
                     (static_cast<double>(end_vel) - static_cast<double>(start_vel)) * t;
        sorted[i]->velocity = static_cast<uint8_t>(
            std::clamp(std::round(vel), 0.0, 127.0));
    }
}

void VelocityLane::scale_selection(const std::vector<Note*>& notes,
                                    double factor) {
    factor = std::clamp(factor, 0.0, 2.0);
    for (Note* n : notes) {
        double new_vel = std::round(static_cast<double>(n->velocity) * factor);
        n->velocity = static_cast<uint8_t>(std::clamp(new_vel, 0.0, 127.0));
    }
}

void VelocityLane::randomize_selection(const std::vector<Note*>& notes,
                                        uint8_t min, uint8_t max) {
    if (min > max) std::swap(min, max);
    min = std::min<uint8_t>(min, 127);
    max = std::min<uint8_t>(max, 127);

    // Use a thread-local RNG seeded once.
    static thread_local std::mt19937 rng(std::random_device{}());

    std::uniform_int_distribution<int> dist(static_cast<int>(min),
                                            static_cast<int>(max));
    for (Note* n : notes) {
        n->velocity = static_cast<uint8_t>(dist(rng));
    }
}

void VelocityLane::reverse_selection(const std::vector<Note*>& notes) {
    if (notes.size() < 2) return;

    // Sort by start_ppqn for a deterministic left-to-right order.
    std::vector<Note*> sorted = notes;
    std::sort(sorted.begin(), sorted.end(),
              [](const Note* a, const Note* b) {
                  return a->start_ppqn < b->start_ppqn;
              });

    // Swap velocities pairwise.
    size_t count = sorted.size();
    for (size_t i = 0; i < count / 2; ++i) {
        uint8_t tmp = sorted[i]->velocity;
        sorted[i]->velocity = sorted[count - 1 - i]->velocity;
        sorted[count - 1 - i]->velocity = tmp;
    }
}

void VelocityLane::apply_curve(const std::vector<Note*>& notes,
                                uint8_t start, uint8_t end, CurveType curve) {
    if (notes.empty()) return;

    std::vector<Note*> sorted = notes;
    std::sort(sorted.begin(), sorted.end(),
              [](const Note* a, const Note* b) {
                  return a->start_ppqn < b->start_ppqn;
              });

    size_t count = sorted.size();
    if (count == 1) {
        sorted[0]->velocity = start;
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(count - 1);
        double shaped_t = curve_factor(t, curve);
        double vel = static_cast<double>(start) +
                     (static_cast<double>(end) - static_cast<double>(start)) * shaped_t;
        sorted[i]->velocity = static_cast<uint8_t>(
            std::clamp(std::round(vel), 0.0, 127.0));
    }
}

// ═════════════════════════════════════════════════════════════════
// Mouse input
// ═════════════════════════════════════════════════════════════════

void VelocityLane::on_mouse_down(float x, float y,
                                  const ViewTransform& view) {
    if (!notes_) return;

    // Convert click position to PPQN and find the note under the cursor.
    uint64_t click_ppqn = view.x_to_ppqn(static_cast<double>(x));

    // Find the note whose visual span contains the x-coordinate.
    // Use the velocity lane's height to compute the velocity value.
    double normalized_y = 1.0 - (static_cast<double>(y) / height_);  // 0..1
    normalized_y = std::clamp(normalized_y, 0.0, 1.0);
    uint8_t click_velocity = static_cast<uint8_t>(
        std::round(normalized_y * 127.0));

    // Search for the note at the click PPQN.
    for (auto& n : notes_->notes()) {
        uint64_t n_end = n.start_ppqn + n.duration_ppqn;
        if (click_ppqn >= n.start_ppqn && click_ppqn < n_end) {
            // Found the note under cursor.
            n.velocity = click_velocity;
            drag_.active  = true;
            drag_.note    = n.id;
            drag_.velocity = click_velocity;
            return;
        }
    }
}

void VelocityLane::on_mouse_move(float x, float y,
                                  const ViewTransform& /*view*/) {
    if (!drag_.active || !notes_) return;

    // Update the velocity of the note being dragged.
    double normalized_y = 1.0 - (static_cast<double>(y) / height_);
    normalized_y = std::clamp(normalized_y, 0.0, 1.0);
    uint8_t new_velocity = static_cast<uint8_t>(
        std::round(normalized_y * 127.0));

    Note* note = notes_->find(drag_.note);
    if (note) {
        note->velocity = new_velocity;
        drag_.velocity = new_velocity;
    }

    (void)x;
}

void VelocityLane::on_mouse_up() {
    drag_.active = false;
}

} // namespace aria
