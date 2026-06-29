#ifndef ARIA_MODEL_CLIP_H
#define ARIA_MODEL_CLIP_H

#include "model/types.h"

#include <cstdint>
#include <string>

namespace aria {

/// Abstract base class for all clip types.
///
/// Provides position, length, looping, fade, gain, and mute
/// metadata common to AudioClips, MidiClips, and AutomationClips.
class Clip {
public:
    virtual ~Clip() = default;

    // ─── Identity ─────────────────────────────────────────────
    ClipID id() const { return id_; }

    // ─── Position & length ────────────────────────────────────
    uint64_t start()  const { return start_ppqn_; }
    virtual uint64_t length() const { return length_ppqn_; }
    uint64_t end()    const { return start_ppqn_ + length(); }

    void set_start(uint64_t ppqn)  { start_ppqn_ = ppqn; }
    virtual void set_length(uint64_t ppqn) { length_ppqn_ = ppqn; }

    // ─── Fade in / out ────────────────────────────────────────
    uint64_t  fade_in()        const { return fade_in_ppqn_; }
    uint64_t  fade_out()       const { return fade_out_ppqn_; }
    FadeShape fade_in_shape()  const { return fade_in_shape_; }
    FadeShape fade_out_shape() const { return fade_out_shape_; }

    void set_fade_in(uint64_t ppqn)           { fade_in_ppqn_ = ppqn; }
    void set_fade_out(uint64_t ppqn)          { fade_out_ppqn_ = ppqn; }
    void set_fade_in_shape(FadeShape s)       { fade_in_shape_ = s; }
    void set_fade_out_shape(FadeShape s)      { fade_out_shape_ = s; }

    // ─── Gain & mute ──────────────────────────────────────────
    double gain()      const { return gain_; }
    bool   is_muted()  const { return muted_; }

    void set_gain(double g)   { gain_ = g; }
    void set_muted(bool m)    { muted_ = m; }

    // ─── Looping ──────────────────────────────────────────────
    bool     is_looping() const { return looping_; }
    uint64_t loop_start() const { return loop_start_ppqn_; }
    uint64_t loop_end()   const { return loop_end_ppqn_; }

    void set_looping(bool enabled)          { looping_ = enabled; }
    void set_loop_range(uint64_t start, uint64_t end) {
        loop_start_ppqn_ = start;
        loop_end_ppqn_   = end;
    }

    // ─── Clip time mapping ────────────────────────────────────
    /// Map an absolute playback position (PPQN) to a position
    /// within this clip, accounting for looping when enabled.
    uint64_t clip_time_at(uint64_t playback_ppqn) const;

    // ─── Fade evaluation ──────────────────────────────────────
    /// Evaluate a fade curve at normalised position t ∈ [0, 1].
    /// Returns the gain multiplier (0.0 = silence, 1.0 = unity).
    static double evaluate_fade(FadeShape shape, double t);

    // ─── Metadata ─────────────────────────────────────────────
    virtual std::string name() const { return name_; }
    uint32_t    color() const { return color_; }

    virtual void set_name(const std::string& n)  { name_ = n; }
    void set_color(uint32_t c)           { color_ = c; }

protected:
    Clip() : id_(next_id_()) {}

private:
    static ClipID next_id_() {
        static uint64_t counter = 1;
        return ClipID{counter++};
    }

    ClipID id_;

    // Position & duration
    uint64_t start_ppqn_  = 0;
    uint64_t length_ppqn_ = 960;  // default 1 bar

    // Fade data
    uint64_t  fade_in_ppqn_  = 0;
    uint64_t  fade_out_ppqn_ = 0;
    FadeShape fade_in_shape_  = FadeShape::None;
    FadeShape fade_out_shape_ = FadeShape::None;

    // Gain & mute
    double gain_  = 1.0;
    bool   muted_ = false;

    // Looping
    bool     looping_         = false;
    uint64_t loop_start_ppqn_ = 0;
    uint64_t loop_end_ppqn_   = 0;

    // Metadata
    std::string name_;
    uint32_t    color_ = 0;
};

} // namespace aria

#endif // ARIA_MODEL_CLIP_H
