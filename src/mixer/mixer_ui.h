#ifndef ARIA_MIXER_UI_H
#define ARIA_MIXER_UI_H

#include <cstdint>
#include <vector>

#include "mixer/mixer.h"

// Forward declarations for Skia types (provided by the UI framework)
struct SkiaCanvas;
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
};

namespace aria {

/// Console-style mixer UI — renders channel strips, faders, meters,
/// and EQ/send panels using Skia.
class MixerUI {
public:
    MixerUI() = default;
    ~MixerUI() = default;

    // ── Channel strip width ─────────────────────────────────────
    enum class ChannelWidth {
        Narrow = 48,
        Normal = 72,
        Wide   = 120,
        Full   = 200
    };

    // ── Layout configuration ────────────────────────────────────
    struct LayoutConfig {
        ChannelWidth width            = ChannelWidth::Normal;
        bool         show_eq          = false;
        bool         show_sends       = false;
        uint32_t     visible_channels = 0;  // 0 = show all
    };

    void set_layout(const LayoutConfig& config);

    // ── Rendering ───────────────────────────────────────────────
    /// Draw the full mixer console within @p bounds.
    /// @p mixer is queried for channel state during rendering.
    void render(SkiaCanvas* canvas, const Rect& bounds, Mixer& mixer);

    // ── Hit testing ─────────────────────────────────────────────
    /// Returns the ChannelID at the given x-coordinate, or
    /// kInvalidChannelID if none.
    ChannelID channel_at(float x) const;

    // ── Interaction ─────────────────────────────────────────────
    void on_mouse_down(float x, float y, Mixer& mixer);
    void on_mouse_move(float x, float y, Mixer& mixer);
    void on_mouse_up();

    // ── Layout metrics ──────────────────────────────────────────
    float total_width() const { return total_width_; }

private:
    LayoutConfig config_;
    std::vector<Rect> channel_rects_;
    float total_width_ = 0.0f;

    // Interaction state
    bool dragging_ = false;
    int32_t drag_channel_ = -1;
    float drag_start_x_ = 0.0f;
    float drag_start_y_ = 0.0f;

    // Internal helpers
    void rebuild_layout(const Rect& bounds, Mixer& mixer);
    void render_channel_strip(SkiaCanvas* canvas, const Rect& rect,
                              ChannelID id, Mixer& mixer);
    void render_fader(SkiaCanvas* canvas, const Rect& rect,
                      float volume_linear, float peak_meter);
    void render_meter(SkiaCanvas* canvas, const Rect& rect, float level_db);
    void render_pan(SkiaCanvas* canvas, const Rect& rect, float pan);
    void render_buttons(SkiaCanvas* canvas, const Rect& rect,
                        bool mute, bool solo, bool phase_invert);
};

} // namespace aria

#endif // ARIA_MIXER_UI_H
