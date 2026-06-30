#ifndef ARIA_API_BINDINGS_H
#define ARIA_API_BINDINGS_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <sol/sol.hpp>

namespace aria {

/// TrackInfo — lightweight descriptor for track API.
struct TrackInfo {
    int64_t id = 0;
    std::string name;
    std::string type; // "audio", "midi", "group", "return", "master"
};

/// TransportAPI — callbacks that bindings invoke.
/// Tests provide mock implementations to verify binding correctness.
struct TransportAPI {
    std::function<void()> play;
    std::function<void()> stop;
    std::function<void()> record;
    std::function<bool()> is_playing;
};

/// TrackAPI — callbacks that bindings invoke.
struct TrackAPI {
    std::function<std::vector<TrackInfo>()> list;
    /// Get track by name. Returns TrackInfo if found, std::nullopt if not.
    std::function<std::optional<TrackInfo>(const std::string& name)> get;
};

/// ClipAPI — callbacks for clip manipulation.
struct ClipAPI {
    /// Move clip by ID to (track, beat). Returns false if track invalid.
    std::function<bool(int64_t clip_id, int64_t track, double beat)> move;
    /// Trim clip by ID to (start, end) in beats. Returns false if invalid.
    std::function<bool(int64_t clip_id, double start, double end)> trim;
    /// Move clip by name. Returns false if track invalid or clip not found.
    std::function<bool(const std::string& name, int64_t track, double beat)> move_by_name;
};

/// TimingAPI — callbacks for timing/position queries.
struct TimingAPI {
    /// Current playback position in beats.
    std::function<double()> position;
    /// Current tempo in BPM.
    std::function<double()> tempo;
    /// Convert beats to seconds at current tempo.
    std::function<double(double beats)> beats_to_seconds;
};

/// Register all `aria.*` Lua API bindings into the given sol::state.
///
/// The transport, tracks, clips, and timing structs hold std::function
/// callbacks that the bindings invoke. In production, these point to real
/// DAW controllers. In tests, they point to mocks.
///
/// Registered namespaces:
///   aria.transport.{play,stop,record,is_playing}()
///   aria.tracks.{list,get(name)}()
///   aria.clips.{move,trim}()
///   aria.timing.{position,tempo,beats_to_seconds}()
/// Register bindings (transport + tracks only — PR 2 compatibility).
void register_api_bindings(sol::state& state,
                           TransportAPI& transport,
                           TrackAPI& tracks);

/// Register full bindings with clips + timing (PR 3+).
void register_api_bindings(sol::state& state,
                           TransportAPI& transport,
                           TrackAPI& tracks,
                           ClipAPI& clips,
                           TimingAPI& timing);

} // namespace aria

#endif // ARIA_API_BINDINGS_H
