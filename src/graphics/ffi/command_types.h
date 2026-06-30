#ifndef ARIA_FFI_COMMAND_TYPES_H
#define ARIA_FFI_COMMAND_TYPES_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace aria {
namespace ffi {

// ── DrawCommandType enum ──────────────────────────────────────────

/// Each value maps to a SkiaCanvas draw method or state operation.
/// Mirrors the TS DrawCommand type definitions.
enum class DrawCommandType {
    kClear,       ///< SkiaCanvas::clear()
    kDrawRect,    ///< SkiaCanvas::drawRect()
    kDrawCircle,  ///< SkiaCanvas::drawCircle()
    kDrawText,    ///< SkiaCanvas::drawTextBlob()
    kFlush,       ///< SkiaCanvas::flush()
    kSave,        ///< SkiaCanvas::save()
    kRestore,     ///< SkiaCanvas::restore()
    kClipRect,    ///< SkiaCanvas::clipRect()
};

/// Convert a DrawCommandType to its JSON string name.
inline const char* command_type_name(DrawCommandType type) {
    switch (type) {
        case DrawCommandType::kClear:      return "clear";
        case DrawCommandType::kDrawRect:   return "draw_rect";
        case DrawCommandType::kDrawCircle: return "draw_circle";
        case DrawCommandType::kDrawText:   return "draw_text";
        case DrawCommandType::kFlush:      return "flush";
        case DrawCommandType::kSave:       return "save";
        case DrawCommandType::kRestore:    return "restore";
        case DrawCommandType::kClipRect:   return "clip_rect";
    }
    return "unknown";
}

/// Parse a string name into a DrawCommandType.
/// Returns std::nullopt if the name is unrecognised.
inline DrawCommandType command_type_from_name(const std::string& name) {
    if (name == "clear")       return DrawCommandType::kClear;
    if (name == "draw_rect")   return DrawCommandType::kDrawRect;
    if (name == "draw_circle") return DrawCommandType::kDrawCircle;
    if (name == "draw_text")   return DrawCommandType::kDrawText;
    if (name == "flush")       return DrawCommandType::kFlush;
    if (name == "save")        return DrawCommandType::kSave;
    if (name == "restore")     return DrawCommandType::kRestore;
    if (name == "clip_rect")   return DrawCommandType::kClipRect;
    return DrawCommandType::kClear;  // fallback (caller should check)
}

// ── DrawCommand::Params ───────────────────────────────────────────

/// Flexible parameter struct shared across all draw command types.
/// Fields are populated based on the command type.
struct DrawCommandParams {
    // Color (used by clear, draw, fill, text)
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    // Stroke color (used by draw_rect, draw_circle)
    float sr = 0.0f, sg = 0.0f, sb = 0.0f, sa = 0.0f;
    float stroke_width   = 0.0f;
    float corner_radius  = 0.0f;

    // Rectangle / clip bounds
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    // Circle
    float cx     = 0.0f;
    float cy     = 0.0f;
    float radius = 0.0f;

    // Text
    std::string text;
    std::string font_family = "sans-serif";
    float       font_size   = 14.0f;
};

// ── DrawCommand ───────────────────────────────────────────────────

/// A single draw command parsed from the JSON command buffer.
struct DrawCommand {
    DrawCommandType type;
    DrawCommandParams params;
};

// ── JSON parsing ─────────────────────────────────────────────────

/// Parse a JSON string containing an array of command objects into
/// a vector of DrawCommand structs.
///
/// Each object must have a "type" field matching one of the command
/// type names. Unknown types are silently skipped. Invalid JSON
/// returns an empty vector.
inline std::vector<DrawCommand> parse_commands(const std::string& json_str) {
    std::vector<DrawCommand> commands;

    if (json_str.empty()) {
        return commands;
    }

    json parsed;
    try {
        parsed = json::parse(json_str);
    } catch (const json::parse_error&) {
        return commands;  // Invalid JSON — return empty
    }

    if (!parsed.is_array()) {
        return commands;
    }

    for (const auto& item : parsed) {
        if (!item.is_object()) continue;

        // Read the type field
        auto type_it = item.find("type");
        if (type_it == item.end() || !type_it->is_string()) continue;

        // Resolve command type
        std::string type_str = type_it->get<std::string>();

        DrawCommandType cmd_type;
        if (type_str == "clear")        cmd_type = DrawCommandType::kClear;
        else if (type_str == "draw_rect")   cmd_type = DrawCommandType::kDrawRect;
        else if (type_str == "draw_circle") cmd_type = DrawCommandType::kDrawCircle;
        else if (type_str == "draw_text")   cmd_type = DrawCommandType::kDrawText;
        else if (type_str == "flush")       cmd_type = DrawCommandType::kFlush;
        else if (type_str == "save")        cmd_type = DrawCommandType::kSave;
        else if (type_str == "restore")     cmd_type = DrawCommandType::kRestore;
        else if (type_str == "clip_rect")   cmd_type = DrawCommandType::kClipRect;
        else continue;  // Unknown type — skip

        // Build params from JSON
        DrawCommandParams params;

        // Float helper: read float from JSON, default to 0 if missing
        auto read_float = [&](const char* key, float default_val = 0.0f) -> float {
            auto it = item.find(key);
            if (it != item.end() && it->is_number()) {
                return it->get<float>();
            }
            return default_val;
        };

        auto read_string = [&](const char* key, const std::string& default_val = "") -> std::string {
            auto it = item.find(key);
            if (it != item.end() && it->is_string()) {
                return it->get<std::string>();
            }
            return default_val;
        };

        // Fill color (all types)
        params.r = read_float("r", 0.0f);
        params.g = read_float("g", 0.0f);
        params.b = read_float("b", 0.0f);
        params.a = read_float("a", 1.0f);

        // Stroke color
        params.sr = read_float("sr", 0.0f);
        params.sg = read_float("sg", 0.0f);
        params.sb = read_float("sb", 0.0f);
        params.sa = read_float("sa", 0.0f);
        params.stroke_width  = read_float("stroke_width", 0.0f);
        params.corner_radius = read_float("corner_radius", 0.0f);

        // Rectangle / clip
        params.x = read_float("x", 0.0f);
        params.y = read_float("y", 0.0f);
        params.w = read_float("w", 0.0f);
        params.h = read_float("h", 0.0f);

        // Circle
        params.cx     = read_float("cx", 0.0f);
        params.cy     = read_float("cy", 0.0f);
        params.radius = read_float("radius", 0.0f);

        // Text
        params.text        = read_string("text");
        params.font_family = read_string("font_family", "sans-serif");
        params.font_size   = read_float("font_size", 14.0f);

        commands.push_back({cmd_type, params});
    }

    return commands;
}

} // namespace ffi
} // namespace aria

#endif // ARIA_FFI_COMMAND_TYPES_H
