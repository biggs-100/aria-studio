#include "expression_lane.h"
#include "piano_roll_canvas.h"  // For ViewTransform

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace aria {

// ═════════════════════════════════════════════════════════════════
// ExpressionCurve implementation
// ═════════════════════════════════════════════════════════════════

void ExpressionCurve::add_point(uint64_t ppqn, double value,
                                InterpolationType interp) {
    value = std::clamp(value, 0.0, 1.0);

    // Replace if a point already exists at this PPQN.
    for (auto& p : points_) {
        if (p.ppqn == ppqn) {
            p.value         = value;
            p.interpolation = interp;
            return;
        }
    }

    // Insert sorted by PPQN.
    Point pt;
    pt.ppqn          = ppqn;
    pt.value         = value;
    pt.interpolation = interp;

    auto it = std::upper_bound(points_.begin(), points_.end(), pt,
                               [](const Point& a, const Point& b) {
                                   return a.ppqn < b.ppqn;
                               });
    points_.insert(it, std::move(pt));
}

bool ExpressionCurve::remove_point(uint64_t ppqn, uint64_t tolerance_ppqn) {
    int best_idx = -1;
    uint64_t best_dist = std::numeric_limits<uint64_t>::max();

    for (size_t i = 0; i < points_.size(); ++i) {
        uint64_t dist = (points_[i].ppqn > ppqn)
                        ? (points_[i].ppqn - ppqn)
                        : (ppqn - points_[i].ppqn);
        if (dist < best_dist) {
            best_dist = dist;
            best_idx  = static_cast<int>(i);
        }
    }

    if (best_idx >= 0 && best_dist <= tolerance_ppqn) {
        points_.erase(points_.begin() + best_idx);
        return true;
    }
    return false;
}

bool ExpressionCurve::move_point(uint64_t old_ppqn, uint64_t new_ppqn,
                                 double new_value) {
    for (size_t i = 0; i < points_.size(); ++i) {
        if (points_[i].ppqn == old_ppqn) {
            Point pt = points_[i];
            pt.ppqn  = new_ppqn;
            pt.value = std::clamp(new_value, 0.0, 1.0);
            points_.erase(points_.begin() + static_cast<ptrdiff_t>(i));

            if (new_ppqn != old_ppqn) {
                // Re-insert sorted.
                add_point(pt.ppqn, pt.value, pt.interpolation);
            } else {
                points_.insert(points_.begin() + static_cast<ptrdiff_t>(i), pt);
            }
            return true;
        }
    }
    return false;
}

bool ExpressionCurve::set_interpolation(uint64_t ppqn, InterpolationType type) {
    for (auto& p : points_) {
        if (p.ppqn == ppqn) {
            p.interpolation = type;
            return true;
        }
    }
    return false;
}

void ExpressionCurve::clear() {
    points_.clear();
}

int ExpressionCurve::find_prev_index(uint64_t ppqn) const {
    if (points_.empty()) return -1;
    if (ppqn < points_.front().ppqn) return -1;

    int idx = 0;
    for (size_t i = 0; i < points_.size(); ++i) {
        if (points_[i].ppqn <= ppqn) {
            idx = static_cast<int>(i);
        } else {
            break;
        }
    }
    return idx;
}

double ExpressionCurve::evaluate(uint64_t ppqn) const {
    if (points_.empty()) return 0.0;
    if (points_.size() == 1) return points_[0].value;

    int prev_idx = find_prev_index(ppqn);
    if (prev_idx < 0) {
        // Before the first point — hold first value.
        return points_.front().value;
    }

    const Point& prev = points_[static_cast<size_t>(prev_idx)];

    if (prev.ppqn == ppqn ||
        static_cast<size_t>(prev_idx) == points_.size() - 1) {
        return prev.value;
    }

    const Point& next = points_[static_cast<size_t>(prev_idx + 1)];

    uint64_t range = next.ppqn - prev.ppqn;
    if (range == 0) return next.value;

    double t = static_cast<double>(ppqn - prev.ppqn) / static_cast<double>(range);
    t = std::clamp(t, 0.0, 1.0);

    switch (prev.interpolation) {
    case InterpolationType::Step:
        return prev.value;

    case InterpolationType::Linear:
        return prev.value + (next.value - prev.value) * t;

    case InterpolationType::EaseIn:
        t = t * t;
        return prev.value + (next.value - prev.value) * t;

    case InterpolationType::EaseOut:
        t = t * (2.0 - t);
        return prev.value + (next.value - prev.value) * t;

    case InterpolationType::EaseInOut:
        t = t * t * (3.0 - 2.0 * t);
        return prev.value + (next.value - prev.value) * t;

    case InterpolationType::Hold:
        if (t < 0.999) return prev.value;
        return next.value;

    case InterpolationType::Bezier: {
        double mt  = 1.0 - t;
        double mt2 = mt * mt;
        double mt3 = mt2 * mt;
        double t2  = t * t;
        double t3  = t2 * t;

        double c1y = prev.value;
        double c2y = next.value;

        return mt3 * prev.value +
               3.0 * mt2 * t * c1y +
               3.0 * mt * t2 * c2y +
               t3 * next.value;
    }

    case InterpolationType::Smooth: {
        double t2 = t * t;
        double t3 = t2 * t;

        double m0, m1;
        if (prev_idx > 0) {
            const Point& p0 = points_[static_cast<size_t>(prev_idx - 1)];
            m0 = (next.value - p0.value) / 2.0;
        } else {
            m0 = next.value - prev.value;
        }
        if (static_cast<size_t>(prev_idx + 2) < points_.size()) {
            const Point& p2 = points_[static_cast<size_t>(prev_idx + 2)];
            m1 = (p2.value - prev.value) / 2.0;
        } else {
            m1 = next.value - prev.value;
        }

        return (2.0 * t3 - 3.0 * t2 + 1.0) * prev.value +
               (t3 - 2.0 * t2 + t) * m0 +
               (-2.0 * t3 + 3.0 * t2) * next.value +
               (t3 - t2) * m1;
    }

    default:
        return prev.value + (next.value - prev.value) * t;
    }
}

std::vector<Vec2> ExpressionCurve::generate_line_strip(
    uint64_t start_ppqn, uint64_t end_ppqn,
    double ppqn_per_px, double val_per_px) const
{
    std::vector<Vec2> strip;
    if (points_.empty()) return strip;

    uint64_t range = (end_ppqn > start_ppqn) ? (end_ppqn - start_ppqn) : 960;
    double pixel_width = static_cast<double>(range) / ppqn_per_px;
    uint32_t segments = std::max<uint32_t>(
        1, static_cast<uint32_t>(std::ceil(pixel_width)));
    segments = std::min<uint32_t>(segments, 8192);

    strip.reserve(segments + 1);

    for (uint32_t i = 0; i <= segments; ++i) {
        uint64_t ppqn = start_ppqn +
            static_cast<uint64_t>(
                static_cast<double>(i) * static_cast<double>(range) /
                static_cast<double>(segments));

        double val = evaluate(ppqn);
        Vec2 pt;
        pt.x = static_cast<double>(ppqn - start_ppqn) / ppqn_per_px;
        pt.y = (1.0 - val) * val_per_px;
        strip.push_back(pt);
    }

    return strip;
}

// ═════════════════════════════════════════════════════════════════
// ExpressionLane implementation
// ═════════════════════════════════════════════════════════════════

void ExpressionLane::set_type(Type type, uint8_t cc_number) {
    type_      = type;
    cc_number_ = (type == CC) ? cc_number : 0;
}

std::string ExpressionLane::label() const {
    switch (type_) {
    case PitchBend:       return "Pitch Bend";
    case CC:              return "CC " + std::to_string(cc_number_);
    case ChannelPressure: return "Channel Pressure";
    case PolyPressure:    return "Poly Pressure";
    case MPETimbre:       return "MPE Timbre (CC 74)";
    case MPEPressure:     return "MPE Pressure";
    case Automation:      return "Automation";
    case Custom:          return "Custom";
    default:              return "Expression";
    }
}

void ExpressionLane::render(SkiaCanvas* /*canvas*/, const Rect& bounds,
                            const ViewTransform& /*view*/,
                            const Theme* /*theme*/) {
    // ─── Render pipeline (stubbed — SkiaCanvas is opaque) ────────
    // 1. Draw lane background fill
    //    canvas->drawRect(bounds.x, bounds.y, bounds.width, bounds.height, bg);
    //
    // 2. Draw label text
    //    canvas->drawText(label(), bounds.x + 4, bounds.y + 12, font, text_color);
    //
    // 3. Generate and draw the curve line strip
    //    auto strip = curve_.generate_line_strip(...)
    //    canvas->drawPolyline(strip, stroke_color, 1.5f);
    //    canvas->drawPolygon(strip, fill_color);  // filled area under curve
    //
    // 4. Draw control points
    //    for each point: canvas->drawCircle(sx, sy, 4, point_color);

    (void)bounds;
}

void ExpressionLane::on_mouse_down(float x, float y,
                                    const ViewTransform& view) {
    if (curve_.empty()) {
        uint64_t ppqn = view.x_to_ppqn(static_cast<double>(x));
        double value  = 1.0 - (static_cast<double>(y) / height_);
        value         = std::clamp(value, 0.0, 1.0);
        curve_.add_point(ppqn, value);

        drag_.active         = true;
        drag_.creating_point = true;
        drag_.point_ppqn     = ppqn;
        drag_.start_value    = value;
        drag_.start_y        = y;
        return;
    }

    // Hit-test existing points (10-pixel radius).
    const double hit_radius = 10.0;
    double nearest_dist     = hit_radius;
    uint64_t nearest_ppqn   = 0;
    double nearest_val      = 0.0;
    bool found              = false;

    for (const auto& p : curve_.points()) {
        double sx = view.ppqn_to_x(p.ppqn);
        double sy = (1.0 - p.value) * static_cast<double>(height_);
        double dx = sx - static_cast<double>(x);
        double dy = sy - static_cast<double>(y);
        double dist = std::sqrt(dx * dx + dy * dy);
        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest_ppqn = p.ppqn;
            nearest_val  = p.value;
            found        = true;
        }
    }

    if (found) {
        drag_.active         = true;
        drag_.dragging_point = true;
        drag_.point_ppqn     = nearest_ppqn;
        drag_.start_value    = nearest_val;
        drag_.start_y        = y;
    } else {
        uint64_t ppqn = view.x_to_ppqn(static_cast<double>(x));
        double value  = 1.0 - (static_cast<double>(y) / height_);
        value         = std::clamp(value, 0.0, 1.0);
        curve_.add_point(ppqn, value);

        drag_.active         = true;
        drag_.creating_point = true;
        drag_.point_ppqn     = ppqn;
        drag_.start_value    = value;
        drag_.start_y        = y;
    }
}

void ExpressionLane::on_mouse_move(float x, float y,
                                    const ViewTransform& view) {
    if (!drag_.active) return;

    uint64_t new_ppqn = view.x_to_ppqn(static_cast<double>(x));
    double new_value  = 1.0 - (static_cast<double>(y) / height_);
    new_value         = std::clamp(new_value, 0.0, 1.0);

    curve_.move_point(drag_.point_ppqn, new_ppqn, new_value);
    drag_.point_ppqn = new_ppqn;
}

void ExpressionLane::on_mouse_up() {
    drag_.active         = false;
    drag_.dragging_point = false;
    drag_.creating_point = false;
}

// ═════════════════════════════════════════════════════════════════
// ExpressionLaneManager implementation
// ═════════════════════════════════════════════════════════════════

void ExpressionLaneManager::add_lane(ExpressionLane::Type type, uint8_t cc) {
    auto lane = std::make_unique<ExpressionLane>();
    lane->set_type(type, cc);
    lanes_.push_back(std::move(lane));
}

void ExpressionLaneManager::remove_lane(uint32_t index) {
    if (index < lanes_.size()) {
        lanes_.erase(lanes_.begin() + static_cast<ptrdiff_t>(index));
    }
}

void ExpressionLaneManager::clear() {
    lanes_.clear();
}

ExpressionLane* ExpressionLaneManager::get_lane(uint32_t index) {
    if (index >= lanes_.size()) return nullptr;
    return lanes_[index].get();
}

const ExpressionLane* ExpressionLaneManager::get_lane(uint32_t index) const {
    if (index >= lanes_.size()) return nullptr;
    return lanes_[index].get();
}

float ExpressionLaneManager::render_all(SkiaCanvas* canvas,
                                         const Rect& bounds,
                                         const ViewTransform& view,
                                         const Theme* theme) {
    float current_y = static_cast<float>(bounds.y);

    for (auto& lane : lanes_) {
        Rect lane_bounds;
        lane_bounds.x      = bounds.x;
        lane_bounds.y      = static_cast<double>(current_y);
        lane_bounds.width  = bounds.width;
        lane_bounds.height = static_cast<double>(lane->height());

        lane->render(canvas, lane_bounds, view, theme);
        current_y += lane->height();
    }

    return current_y - static_cast<float>(bounds.y);
}

int ExpressionLaneManager::handle_mouse_down(float x, float y,
                                              const ViewTransform& view) {
    // Find the lane under the y-coordinate.
    float acc_y = 0.0f;
    for (uint32_t i = 0; i < lanes_.size(); ++i) {
        float lane_h = lanes_[i]->height();
        if (y >= acc_y && y < acc_y + lane_h) {
            active_lane_          = static_cast<int>(i);
            drag_active_           = true;
            active_lane_y_offset_  = acc_y;
            float local_y          = y - acc_y;
            lanes_[i]->on_mouse_down(x, local_y, view);
            return static_cast<int>(i);
        }
        acc_y += lane_h;
    }
    return -1;
}

void ExpressionLaneManager::handle_mouse_move(float x, float y,
                                               const ViewTransform& view) {
    if (!drag_active_ || active_lane_ < 0 ||
        static_cast<size_t>(active_lane_) >= lanes_.size()) {
        return;
    }

    float local_y = y - active_lane_y_offset_;
    lanes_[static_cast<size_t>(active_lane_)]->on_mouse_move(x, local_y, view);
}

void ExpressionLaneManager::handle_mouse_up() {
    if (active_lane_ >= 0 &&
        static_cast<size_t>(active_lane_) < lanes_.size()) {
        lanes_[static_cast<size_t>(active_lane_)]->on_mouse_up();
    }

    drag_active_          = false;
    active_lane_          = -1;
    active_lane_y_offset_ = 0.0f;
}

float ExpressionLaneManager::total_height() const {
    float h = 0.0f;
    for (const auto& lane : lanes_) {
        h += lane->height();
    }
    return h;
}

} // namespace aria
