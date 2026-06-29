#ifndef ARIA_MODEL_TYPES_H
#define ARIA_MODEL_TYPES_H

#include <cstdint>
#include <functional>

namespace aria {

// ─── TrackID ─────────────────────────────────────────────────────

/// Unique identifier for a track. Implicitly convertible from
/// uint64_t for backward compatibility with mixers and other
/// subsystems that use raw integer IDs.
struct TrackID {
    uint64_t value = 0;

    constexpr TrackID() = default;
    // NOLINTNEXTLINE(google-explicit-constructor) — backward compat
    constexpr TrackID(uint64_t v) : value(v) {}  // NOLINT

    bool operator==(const TrackID& o) const { return value == o.value; }
    bool operator!=(const TrackID& o) const { return value != o.value; }
    bool operator==(uint64_t v) const { return value == v; }
    bool operator!=(uint64_t v) const { return value != v; }
};

static constexpr TrackID kInvalidTrackID{UINT64_MAX};

} // namespace aria

// ─── Hash specializations (for unordered_map use) ──────────────
namespace std {
template<> struct hash<aria::TrackID> {
    size_t operator()(const aria::TrackID& id) const noexcept {
        return hash<uint64_t>{}(id.value);
    }
};
} // namespace std

namespace aria {

// ─── ClipID ──────────────────────────────────────────────────────

/// Unique identifier for a clip.
struct ClipID {
    uint64_t value = 0;

    constexpr ClipID() = default;
    constexpr explicit ClipID(uint64_t v) : value(v) {}

    bool operator==(const ClipID& o) const { return value == o.value; }
    bool operator!=(const ClipID& o) const { return value != o.value; }
};

// ─── FadeShape ───────────────────────────────────────────────────

/// Fade curve shapes for clip fade-in and fade-out.
enum class FadeShape : uint8_t {
    None,
    LinearIn,
    LinearOut,
    EqualPowerIn,
    EqualPowerOut,
    ExponentialIn,
    ExponentialOut
};

// ─── TrackType ───────────────────────────────────────────────────

/// Track specialization types.
enum class TrackType : uint8_t {
    Audio,
    MIDI,
    Group,
    VCA,
    Return,
    Master
};

// ─── SceneID ─────────────────────────────────────────────────────

/// Unique identifier for a session scene.
struct SceneID {
    uint32_t value = 0;

    constexpr SceneID() = default;
    constexpr explicit SceneID(uint32_t v) : value(v) {}

    bool operator==(const SceneID& o) const { return value == o.value; }
    bool operator!=(const SceneID& o) const { return value != o.value; }
};

} // namespace aria

// ─── Hash specializations ─────────────────────────────────────────
namespace std {
template<> struct hash<aria::SceneID> {
    size_t operator()(const aria::SceneID& id) const noexcept {
        return hash<uint32_t>{}(id.value);
    }
};

template<> struct hash<pair<aria::ClipID, aria::ClipID>> {
    size_t operator()(const pair<aria::ClipID, aria::ClipID>& p) const noexcept {
        auto h1 = hash<uint64_t>{}(p.first.value);
        auto h2 = hash<uint64_t>{}(p.second.value);
        return h1 ^ (h2 << 1);
    }
};
} // namespace std

#endif // ARIA_MODEL_TYPES_H
