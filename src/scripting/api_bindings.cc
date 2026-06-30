#include "api_bindings.h"

#include <sol/sol.hpp>

namespace aria {

namespace {

/// Convert a TrackInfo to a Lua table.
sol::table track_to_table(sol::state& state, const TrackInfo& track) {
    sol::table tbl = state.create_table();
    tbl["id"] = track.id;
    tbl["name"] = track.name;
    tbl["type"] = track.type;
    return tbl;
}

/// Default empty TimingAPI (used when no timing struct is provided).
static TimingAPI default_timing() {
    TimingAPI t;
    t.position = []() -> double { return 0.0; };
    t.tempo = []() -> double { return 120.0; };
    t.beats_to_seconds = [](double beats) -> double {
        return (beats / 120.0) * 60.0;
    };
    return t;
}

/// Default empty ClipAPI (used when no clips struct is provided).
static ClipAPI default_clips() {
    ClipAPI c;
    c.move = [](int64_t, int64_t, double) -> bool { return false; };
    c.trim = [](int64_t, double, double) -> bool { return false; };
    c.move_by_name = [](const std::string&, int64_t, double) -> bool { return false; };
    return c;
}

} // anonymous namespace

void register_api_bindings(sol::state& state,
                           TransportAPI& transport,
                           TrackAPI& tracks) {

    // ── aria namespace ──────────────────────────────────────────
    sol::table aria = state["aria"].get_or_create<sol::table>();

    // ── aria.transport.* ────────────────────────────────────────
    sol::table transport_tbl = aria.create_named("transport");
    transport_tbl.set_function("play",     [&transport]() { transport.play(); });
    transport_tbl.set_function("stop",     [&transport]() { transport.stop(); });
    transport_tbl.set_function("record",   [&transport]() { transport.record(); });
    transport_tbl.set_function("is_playing", [&transport]() -> bool {
        return transport.is_playing();
    });

    // ── aria.tracks.* ───────────────────────────────────────────
    sol::table tracks_tbl = aria.create_named("tracks");

    tracks_tbl.set_function("list", [&tracks, &state]() -> sol::table {
        auto vec = tracks.list();
        sol::table result = state.create_table();
        for (size_t i = 0; i < vec.size(); ++i) {
            result[i + 1] = track_to_table(state, vec[i]);
        }
        return result;
    });

    tracks_tbl.set_function("get", [&tracks, &state](const std::string& name) -> sol::object {
        // Try to get track info. If not found, return nil.
        auto result = tracks.get(name);
        if (result.has_value()) {
            return sol::make_object(state, track_to_table(state, result.value()));
        }
        return sol::nil;
    });

    // ── aria.clips.* ───────────────────────────────────────────
    // Installed with default empty handlers; real handlers are set
    // when the 5-param overload is used.
    ClipAPI default_clips_api = default_clips();
    sol::table clips_tbl = aria.create_named("clips");
    clips_tbl.set_function("move",
        [&default_clips_api](int64_t clip_id, int64_t track, double beat) -> bool {
            return default_clips_api.move(clip_id, track, beat);
        });
    clips_tbl.set_function("trim",
        [&default_clips_api](int64_t clip_id, double start, double end) -> bool {
            return default_clips_api.trim(clip_id, start, end);
        });

    // ── aria.timing.* ──────────────────────────────────────────
    TimingAPI default_timing_api = default_timing();
    sol::table timing_tbl = aria.create_named("timing");
    timing_tbl.set_function("position",
        [&default_timing_api]() -> double {
            return default_timing_api.position();
        });
    timing_tbl.set_function("tempo",
        [&default_timing_api]() -> double {
            return default_timing_api.tempo();
        });
    timing_tbl.set_function("beats_to_seconds",
        [&default_timing_api](double beats) -> double {
            return default_timing_api.beats_to_seconds(beats);
        });
}

void register_api_bindings(sol::state& state,
                           TransportAPI& transport,
                           TrackAPI& tracks,
                           ClipAPI& clips,
                           TimingAPI& timing) {
    sol::table aria = state["aria"].get_or_create<sol::table>();

    // ── aria.transport.* ────────────────────────────────────────
    sol::table transport_tbl = aria.create_named("transport");
    transport_tbl.set_function("play",     [&transport]() { transport.play(); });
    transport_tbl.set_function("stop",     [&transport]() { transport.stop(); });
    transport_tbl.set_function("record",   [&transport]() { transport.record(); });
    transport_tbl.set_function("is_playing", [&transport]() -> bool {
        return transport.is_playing();
    });

    // ── aria.tracks.* ───────────────────────────────────────────
    sol::table tracks_tbl = aria.create_named("tracks");
    tracks_tbl.set_function("list", [&tracks, &state]() -> sol::table {
        auto vec = tracks.list();
        sol::table result = state.create_table();
        for (size_t i = 0; i < vec.size(); ++i) {
            result[i + 1] = track_to_table(state, vec[i]);
        }
        return result;
    });
    tracks_tbl.set_function("get", [&tracks, &state](const std::string& name) -> sol::object {
        auto result = tracks.get(name);
        if (result.has_value()) {
            return sol::make_object(state, track_to_table(state, result.value()));
        }
        return sol::nil;
    });

    // ── aria.clips.* ────────────────────────────────────────────
    sol::table clips_tbl = aria.create_named("clips");
    clips_tbl.set_function("move",
        [&clips](int64_t clip_id, int64_t track, double beat) -> bool {
            return clips.move(clip_id, track, beat);
        });
    clips_tbl.set_function("trim",
        [&clips](int64_t clip_id, double start, double end) -> bool {
            return clips.trim(clip_id, start, end);
        });

    // ── aria.timing.* ───────────────────────────────────────────
    sol::table timing_tbl = aria.create_named("timing");
    timing_tbl.set_function("position",
        [&timing]() -> double { return timing.position(); });
    timing_tbl.set_function("tempo",
        [&timing]() -> double { return timing.tempo(); });
    timing_tbl.set_function("beats_to_seconds",
        [&timing](double beats) -> double {
            return timing.beats_to_seconds(beats);
        });
}

} // namespace aria
