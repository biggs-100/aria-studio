#include "mixer/mixer_ui.h"

#include <algorithm>
#include <cmath>

namespace aria {

// ═══════════════════════════════════════════════════════════════════
//  Layout
// ═══════════════════════════════════════════════════════════════════

void MixerUI::set_layout(const LayoutConfig& config) {
    config_ = config;
}

void MixerUI::rebuild_layout(const Rect& bounds, Mixer& mixer) {
    channel_rects_.clear();

    auto all_channels = mixer.all_channels();
    float strip_w = static_cast<float>(static_cast<int>(config_.width));
    float x = bounds.x;

    for (size_t i = 0; i < all_channels.size(); ++i) {
        // Respect visible_channels limit (0 = all)
        if (config_.visible_channels > 0 && i >= config_.visible_channels) break;

        Rect r;
        r.x = x;
        r.y = bounds.y;
        r.w = strip_w;
        r.h = bounds.h;
        channel_rects_.push_back(r);

        x += strip_w;
    }

    total_width_ = x - bounds.x;
}

// ═══════════════════════════════════════════════════════════════════
//  Rendering
// ═══════════════════════════════════════════════════════════════════

void MixerUI::render(SkiaCanvas* canvas, const Rect& bounds, Mixer& mixer) {
    if (!canvas) return;

    rebuild_layout(bounds, mixer);

    auto all_channels = mixer.all_channels();
    size_t limit = config_.visible_channels > 0
        ? std::min(static_cast<size_t>(config_.visible_channels), all_channels.size())
        : all_channels.size();

    for (size_t i = 0; i < limit && i < channel_rects_.size(); ++i) {
        render_channel_strip(canvas, channel_rects_[i], all_channels[i], mixer);
    }
}

void MixerUI::render_channel_strip(SkiaCanvas* canvas, const Rect& rect,
                                    ChannelID id, Mixer& mixer) {
    auto* ch = mixer.get_channel(id);
    if (!ch) return;

    // ── Background ──────────────────────────────────────────────
    // (Drawing calls use hypothetical Skia API — actual rendering
    //  is delegated to the UI layer which provides SkiaCanvas.)

    // ── Channel label ───────────────────────────────────────────
    // canvas->draw_text(rect.x + 2, rect.y + 12, ch->name().c_str(), ...);

    // ── Buttons (mute, solo, phase) ─────────────────────────────
    float btn_y = rect.y + 16;
    Rect btn_rect{rect.x + 4, btn_y, rect.w - 8, 16};
    render_buttons(canvas, btn_rect,
                   ch->is_muted(), ch->is_soloed(), ch->phase_inverted());

    // ── Pan ─────────────────────────────────────────────────────
    float pan_y = btn_y + 20;
    Rect pan_rect{rect.x + 4, pan_y, rect.w - 8, 12};
    render_pan(canvas, pan_rect, static_cast<float>(ch->pan()));

    // ── Meter ───────────────────────────────────────────────────
    float meter_y = pan_y + 16;
    float meter_h = rect.h - meter_y - 60;  // leave room for fader
    Rect meter_rect{rect.x + rect.w * 0.3f, meter_y,
                    rect.w * 0.4f, meter_h};
    render_meter(canvas, meter_rect, ch->peak_left());

    // ── Fader ───────────────────────────────────────────────────
    float fader_y = rect.h - 52;
    Rect fader_rect{rect.x + 4, rect.y + fader_y, rect.w - 8, 48};
    render_fader(canvas, fader_rect,
                 static_cast<float>(ch->linear_volume()),
                 ch->peak_left());

    // ── EQ toggle ───────────────────────────────────────────────
    if (config_.show_eq) {
        float eq_y = fader_y - 80;
        // canvas->draw_text(rect.x + 2, rect.y + eq_y, "EQ", ...);
    }

    // ── Sends toggle ────────────────────────────────────────────
    if (config_.show_sends) {
        float send_y = fader_y - 60;
        // canvas->draw_text(rect.x + 2, rect.y + send_y, "Sends", ...);
    }
}

void MixerUI::render_fader(SkiaCanvas* canvas, const Rect& rect,
                            float volume_linear, float peak_meter) {
    (void)canvas;
    (void)rect;
    (void)volume_linear;
    (void)peak_meter;
    // Fader track and thumb drawing delegated to Skia.
    // Track: vertical rectangle with dB scale markings.
    // Thumb: small rounded rectangle positioned by volume_linear.
}

void MixerUI::render_meter(SkiaCanvas* canvas, const Rect& rect, float level_db) {
    (void)canvas;
    (void)rect;
    (void)level_db;
    // Vertical bar meter with gradient (green → yellow → red).
    // level_db maps to fill height; -60 dB = empty, 0 dB = top.
}

void MixerUI::render_pan(SkiaCanvas* canvas, const Rect& rect, float pan) {
    (void)canvas;
    (void)rect;
    (void)pan;
    // Horizontal pan indicator: centered knob at position based on pan value.
}

void MixerUI::render_buttons(SkiaCanvas* canvas, const Rect& rect,
                              bool mute, bool solo, bool phase_invert) {
    (void)canvas;
    (void)rect;
    (void)mute;
    (void)solo;
    (void)phase_invert;
    // Three small toggle buttons: M, S, Φ with highlight states.
}

// ═══════════════════════════════════════════════════════════════════
//  Hit Testing & Interaction
// ═══════════════════════════════════════════════════════════════════

ChannelID MixerUI::channel_at(float x) const {
    for (size_t i = 0; i < channel_rects_.size(); ++i) {
        const auto& r = channel_rects_[i];
        if (x >= r.x && x < r.x + r.w) {
            // Return a simulated ID (real implementation would map index→ID)
            return static_cast<ChannelID>(i);
        }
    }
    return kInvalidChannelID;
}

void MixerUI::on_mouse_down(float x, float y, Mixer& mixer) {
    (void)mixer;
    ChannelID id = channel_at(x);
    if (id == kInvalidChannelID) return;

    drag_channel_ = static_cast<int32_t>(id);
    drag_start_x_ = x;
    drag_start_y_ = y;
    dragging_ = true;
}

void MixerUI::on_mouse_move(float x, float y, Mixer& mixer) {
    if (!dragging_ || drag_channel_ < 0) return;

    float dy = y - drag_start_y_;
    ChannelID id = static_cast<ChannelID>(drag_channel_);
    auto* ch = mixer.get_channel(id);
    if (ch) {
        // Fader drag: delta y → volume change
        double db_change = static_cast<double>(-dy) * 0.5; // 2px ≈ 1 dB
        double new_vol = ch->volume() + db_change;
        ch->set_volume(new_vol);
    }

    drag_start_x_ = x;
    drag_start_y_ = y;
}

void MixerUI::on_mouse_up() {
    dragging_ = false;
    drag_channel_ = -1;
}

} // namespace aria
