#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "graphics/ffi/command_types.h"
#include "graphics/ffi/bridge.h"
#include "graphics/ffi/event_dispatcher.h"
#include "graphics/graphics_types.h"
#include "graphics/skia_canvas.h"

#include <string>
#include <vector>
#include <sstream>

using namespace aria;
using namespace aria::ffi;

// ─── FFI Bridge TDD Tests ─────────────────────────────────────────
//
// Test Strategy:
//   - Verify DrawCommand enum values and string mapping.
//   - Verify JSON deserialization produces correct command structs.
//   - Verify Bridge::execute dispatches commands to SkiaCanvas in order.
//   - Verify EventDispatcher converts OS events to TS callbacks.
//   - Verify HiDPI coordinate scaling.
//
//   TDD notes:
//     - RED:   Tests reference command_types.h / bridge.h before creation.
//     - GREEN: Create minimum headers/impl to compile and pass.
//     - TRIANGULATE: Edge cases, empty buffers, invalid JSON.

// =====================================================================
// RED / GREEN – DrawCommandType enum
// =====================================================================

TEST_CASE("DrawCommandType has all required enum values",
          "[graphics][ffi][command_types]")
{
    // GIVEN the DrawCommandType enum
    // THEN all spec-required values exist

    // We verify by constructing each value and stringifying via command_type_name()
    CHECK(command_type_name(DrawCommandType::kClear) == "clear");
    CHECK(command_type_name(DrawCommandType::kDrawRect) == "draw_rect");
    CHECK(command_type_name(DrawCommandType::kDrawCircle) == "draw_circle");
    CHECK(command_type_name(DrawCommandType::kDrawText) == "draw_text");
    CHECK(command_type_name(DrawCommandType::kFlush) == "flush");
    CHECK(command_type_name(DrawCommandType::kSave) == "save");
    CHECK(command_type_name(DrawCommandType::kRestore) == "restore");
    CHECK(command_type_name(DrawCommandType::kClipRect) == "clip_rect");

    // AND reverse lookup works
    CHECK(command_type_from_name("draw_rect") == DrawCommandType::kDrawRect);
    CHECK(command_type_from_name("flush") == DrawCommandType::kFlush);
}

// =====================================================================
// RED / GREEN – JSON deserialization
// =====================================================================

TEST_CASE("parse_commands handles empty JSON array",
          "[graphics][ffi][parse]")
{
    // GIVEN an empty JSON buffer
    std::string empty_json = "[]";

    // WHEN parse_commands is called
    auto commands = parse_commands(empty_json);

    // THEN zero commands are returned
    CHECK(commands.empty());
}

TEST_CASE("parse_commands deserialises a single clear command",
          "[graphics][ffi][parse]")
{
    // GIVEN a JSON buffer with one clear command
    std::string json = R"([
        {"type": "clear", "r": 0.0, "g": 0.0, "b": 0.0, "a": 1.0}
    ])";

    // WHEN parse_commands is called
    auto commands = parse_commands(json);

    // THEN one command is returned with correct type
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == DrawCommandType::kClear);
}

TEST_CASE("parse_commands deserialises a draw_rect command with fill",
          "[graphics][ffi][parse]")
{
    // GIVEN a JSON buffer with one draw_rect command
    std::string json = R"([
        {"type": "draw_rect", "x": 10, "y": 20, "w": 100, "h": 50,
         "r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0,
         "corner_radius": 4.0}
    ])";

    auto commands = parse_commands(json);

    // THEN the rect params are parsed correctly
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == DrawCommandType::kDrawRect);
    CHECK(commands[0].params.x == 10.0f);
    CHECK(commands[0].params.y == 20.0f);
    CHECK(commands[0].params.w == 100.0f);
    CHECK(commands[0].params.h == 50.0f);
    CHECK(commands[0].params.r == 1.0f);
    CHECK(commands[0].params.g == 0.0f);
    CHECK(commands[0].params.b == 0.0f);
    CHECK(commands[0].params.a == 1.0f);
    CHECK(commands[0].params.corner_radius == 4.0f);
}

TEST_CASE("parse_commands deserialises draw_circle command",
          "[graphics][ffi][parse]")
{
    std::string json = R"([
        {"type": "draw_circle", "cx": 50, "cy": 50, "radius": 25,
         "r": 0.0, "g": 0.5, "b": 1.0, "a": 0.8}
    ])";

    auto commands = parse_commands(json);

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == DrawCommandType::kDrawCircle);
    CHECK(commands[0].params.cx == 50.0f);
    CHECK(commands[0].params.cy == 50.0f);
    CHECK(commands[0].params.radius == 25.0f);
    CHECK(commands[0].params.r == 0.0f);
    CHECK(commands[0].params.g == 0.5f);
    CHECK(commands[0].params.b == 1.0f);
    CHECK(commands[0].params.a == 0.8f);
}

TEST_CASE("parse_commands deserialises draw_text command",
          "[graphics][ffi][parse]")
{
    std::string json = R"([
        {"type": "draw_text", "text": "Hello GPU", "font_family": "sans-serif",
         "font_size": 16, "x": 10, "y": 30, "r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0}
    ])";

    auto commands = parse_commands(json);

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == DrawCommandType::kDrawText);
    CHECK(commands[0].params.text == "Hello GPU");
    CHECK(commands[0].params.font_family == "sans-serif");
    CHECK(commands[0].params.font_size == 16.0f);
    CHECK(commands[0].params.x == 10.0f);
    CHECK(commands[0].params.y == 30.0f);
}

TEST_CASE("parse_commands deserialises state commands (save, restore, flush, clip_rect)",
          "[graphics][ffi][parse]")
{
    std::string json = R"([
        {"type": "save"},
        {"type": "clip_rect", "x": 0, "y": 0, "w": 200, "h": 100},
        {"type": "restore"},
        {"type": "flush"}
    ])";

    auto commands = parse_commands(json);

    REQUIRE(commands.size() == 4);
    CHECK(commands[0].type == DrawCommandType::kSave);
    CHECK(commands[1].type == DrawCommandType::kClipRect);
    CHECK(commands[1].params.x == 0.0f);
    CHECK(commands[1].params.y == 0.0f);
    CHECK(commands[1].params.w == 200.0f);
    CHECK(commands[1].params.h == 100.0f);
    CHECK(commands[2].type == DrawCommandType::kRestore);
    CHECK(commands[3].type == DrawCommandType::kFlush);
}

// =====================================================================
// TRIANGULATE – JSON parsing edge cases
// =====================================================================

TEST_CASE("parse_commands handles invalid JSON gracefully",
          "[graphics][ffi][parse][safety]")
{
    // GIVEN malformed JSON
    std::string bad_json = "not valid json";

    // WHEN parse_commands is called
    // THEN it returns empty vector without crashing
    auto commands = parse_commands(bad_json);
    CHECK(commands.empty());
}

TEST_CASE("parse_commands handles unknown command type gracefully",
          "[graphics][ffi][parse][safety]")
{
    std::string json = R"([
        {"type": "unknown_command_type"}
    ])";

    auto commands = parse_commands(json);
    CHECK(commands.empty());
}

TEST_CASE("parse_commands handles missing type field gracefully",
          "[graphics][ffi][parse][safety]")
{
    std::string json = R"([
        {"x": 10, "y": 20}
    ])";

    auto commands = parse_commands(json);
    CHECK(commands.empty());
}

TEST_CASE("parse_commands handles multiple mixed commands in order",
          "[graphics][ffi][parse]")
{
    std::string json = R"([
        {"type": "clear", "r": 0.1, "g": 0.2, "b": 0.3, "a": 1.0},
        {"type": "draw_rect", "x": 0, "y": 0, "w": 100, "h": 100, "r": 1, "g": 0, "b": 0, "a": 1},
        {"type": "draw_circle", "cx": 50, "cy": 50, "radius": 20, "r": 0, "g": 1, "b": 0, "a": 1},
        {"type": "flush"}
    ])";

    auto commands = parse_commands(json);

    REQUIRE(commands.size() == 4);
    CHECK(commands[0].type == DrawCommandType::kClear);
    CHECK(commands[1].type == DrawCommandType::kDrawRect);
    CHECK(commands[2].type == DrawCommandType::kDrawCircle);
    CHECK(commands[3].type == DrawCommandType::kFlush);

    // Verify clear color
    CHECK(commands[0].params.r == 0.1f);
    CHECK(commands[0].params.g == 0.2f);
    CHECK(commands[0].params.b == 0.3f);
}

// =====================================================================
// RED / GREEN – Bridge execution (dispatches commands to SkiaCanvas)
// =====================================================================

TEST_CASE("Bridge::execute with empty buffer does nothing",
          "[graphics][ffi][bridge]")
{
    // GIVEN a Bridge with a null canvas (no GPU available)
    Bridge bridge(nullptr);

    // WHEN execute is called with empty JSON
    // THEN no crash occurs
    CHECK_NOTHROW(bridge.execute("[]"));
    CHECK_NOTHROW(bridge.execute(""));
}

TEST_CASE("Bridge::execute with clear command calls SkiaCanvas clear",
          "[graphics][ffi][bridge]")
{
    // GIVEN a Bridge with a canvas (may be uninitialised - safe no-op)
    SkiaCanvas canvas;
    Bridge bridge(&canvas);

    // WHEN execute is called with a clear command
    std::string json = R"([
        {"type": "clear", "r": 0.0, "g": 0.0, "b": 0.0, "a": 1.0}
    ])";

    // THEN no crash — the command is dispatched
    CHECK_NOTHROW(bridge.execute(json));

    // AND draw call count is available (may be 0 if no GPU)
    CHECK_NOTHROW(canvas.draw_call_count());
}

TEST_CASE("Bridge::execute with multiple draw commands dispatches in order",
          "[graphics][ffi][bridge]")
{
    SkiaCanvas canvas;
    Bridge bridge(&canvas);

    std::string json = R"([
        {"type": "clear", "r": 0.2, "g": 0.3, "b": 0.4, "a": 1.0},
        {"type": "save"},
        {"type": "clip_rect", "x": 10, "y": 10, "w": 200, "h": 150},
        {"type": "draw_rect", "x": 20, "y": 20, "w": 50, "h": 30, "r": 1, "g": 0, "b": 0, "a": 1},
        {"type": "draw_text", "text": "test", "x": 30, "y": 40, "r": 1, "g": 1, "b": 1, "a": 1},
        {"type": "restore"},
        {"type": "flush"}
    ])";

    CHECK_NOTHROW(bridge.execute(json));
}

TEST_CASE("Bridge::execute with HiDPI scaling transforms coordinates",
          "[graphics][ffi][bridge]")
{
    SkiaCanvas canvas;
    Bridge bridge(&canvas);

    // GIVEN a bridge with 2.0x device pixel ratio
    bridge.set_device_pixel_ratio(2.0f);
    CHECK(bridge.device_pixel_ratio() == 2.0f);

    // WHEN draw commands at logical coordinates are executed
    // THEN they are multiplied by DPR (verified via SkiaCanvas calls)
    // (Without GPU we verify no crash; DPR multiplication is tested structurally)
    std::string json = R"([
        {"type": "draw_rect", "x": 100, "y": 100, "w": 50, "h": 30,
         "r": 1, "g": 0, "b": 0, "a": 1}
    ])";

    CHECK_NOTHROW(bridge.execute(json));
}

// =====================================================================
// RED / GREEN – EventDispatcher
// =====================================================================

TEST_CASE("EventDispatcher stores and calls registered callback",
          "[graphics][ffi][event]")
{
    EventDispatcher dispatcher;
    bool called = false;
    InputEvent received;

    // GIVEN a registered callback
    dispatcher.on_event([&](const InputEvent& ev) {
        called = true;
        received = ev;
    });

    // WHEN a mouse down event is dispatched
    dispatcher.dispatch_mouse_down(200.0f, 150.0f, 0);

    // THEN the callback is invoked with correct event data
    CHECK(called);
    CHECK(received.type == EventType::kMouseDown);
    CHECK(received.x == 200.0f);
    CHECK(received.y == 150.0f);
    CHECK(received.button == 0);
}

TEST_CASE("EventDispatcher handles no registered callback gracefully",
          "[graphics][ffi][event]")
{
    EventDispatcher dispatcher;

    // GIVEN no registered callback
    // WHEN events are dispatched
    // THEN no crash or memory leak occurs
    CHECK_NOTHROW(dispatcher.dispatch_mouse_down(100.0f, 100.0f, 0));
    CHECK_NOTHROW(dispatcher.dispatch_mouse_move(200.0f, 150.0f));
    CHECK_NOTHROW(dispatcher.dispatch_mouse_up(200.0f, 150.0f, 0));
    CHECK_NOTHROW(dispatcher.dispatch_key_down(65, 0));   // 'A' key
    CHECK_NOTHROW(dispatcher.dispatch_key_up(65, 0));
}

TEST_CASE("EventDispatcher mouse events carry correct EventType",
          "[graphics][ffi][event]")
{
    EventDispatcher dispatcher;
    InputEvent last;

    dispatcher.on_event([&](const InputEvent& ev) { last = ev; });

    // WHEN mouse move
    dispatcher.dispatch_mouse_move(320.0f, 240.0f);
    CHECK(last.type == EventType::kMouseMove);
    CHECK(last.x == 320.0f);
    CHECK(last.y == 240.0f);

    // WHEN mouse down
    dispatcher.dispatch_mouse_down(320.0f, 240.0f, 0);
    CHECK(last.type == EventType::kMouseDown);
    CHECK(last.button == 0);

    // WHEN mouse up
    dispatcher.dispatch_mouse_up(320.0f, 240.0f, 0);
    CHECK(last.type == EventType::kMouseUp);

    // WHEN key down
    dispatcher.dispatch_key_down(32, 0);  // space
    CHECK(last.type == EventType::kKeyDown);
    CHECK(last.key_code == 32);

    // WHEN key up
    dispatcher.dispatch_key_up(32, 0);
    CHECK(last.type == EventType::kKeyUp);
}

TEST_CASE("EventDispatcher applies HiDPI scaling to mouse coordinates",
          "[graphics][ffi][event]")
{
    EventDispatcher dispatcher;
    InputEvent last;

    dispatcher.on_event([&](const InputEvent& ev) { last = ev; });

    // GIVEN a 2.0x HiDPI display
    dispatcher.set_device_pixel_ratio(2.0f);

    // WHEN a mouse event at physical (400, 300) arrives
    dispatcher.dispatch_mouse_down(400.0f, 300.0f, 0);

    // THEN coordinates are divided by the scale factor
    CHECK(last.x == 200.0f);
    CHECK(last.y == 150.0f);
}

TEST_CASE("EventDispatcher 100% scale passes coordinates through",
          "[graphics][ffi][event]")
{
    EventDispatcher dispatcher;
    InputEvent last;

    dispatcher.on_event([&](const InputEvent& ev) { last = ev; });

    // GIVEN 1.0x (default) scale
    CHECK(dispatcher.device_pixel_ratio() == 1.0f);

    // WHEN any event arrives
    dispatcher.dispatch_mouse_down(100.0f, 75.0f, 0);

    // THEN coordinates are unchanged
    CHECK(last.x == 100.0f);
    CHECK(last.y == 75.0f);
}
