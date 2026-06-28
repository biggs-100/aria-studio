#ifndef ARIA_PIANOROLL_TOOL_MANAGER_H
#define ARIA_PIANOROLL_TOOL_MANAGER_H

#include "note.h"
#include "note_collection.h"
#include "piano_roll_canvas.h"  // For Tool, ViewTransform
#include "selection_manager.h"
#include "snap_system.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace aria {

// ─── Cursor ─────────────────────────────────────────────────────

/// Mouse cursor types used by tools.
enum class Cursor : uint8_t {
    Default,
    Crosshair,
    Pointer,
    Text,
    IBeam,
    Move,
    NotAllowed,
    NSResize,
    EWResize,
    NEResize,
    NWResize,
    Grab,
    Pen,
    Eraser,
    Paint,
    Cut,
    Glue,
    Mute,
    Ramp,
    Measure,
    ZoomIn,
    ZoomOut,
    Wait
};

// ─── MouseEvent ─────────────────────────────────────────────────

/// Mouse event data passed to tool handlers.
struct MouseEvent {
    float       x = 0.0f;           // Screen pixel X
    float       y = 0.0f;           // Screen pixel Y
    float       dx = 0.0f;          // Delta X from last move
    float       dy = 0.0f;          // Delta Y from last move
    bool        alt = false;        // Alt held
    bool        ctrl = false;       // Ctrl held
    bool        shift = false;      // Shift held
    bool        left = true;        // Left button
    bool        right = false;      // Right button
    bool        middle = false;     // Middle button
    bool        double_click = false;
    bool        consumed = false;   // Set true to prevent fallthrough
};

// ─── ToolHandler base ──────────────────────────────────────────

/// Base class for all tool interaction handlers.
class ToolHandler {
public:
    virtual ~ToolHandler() = default;

    virtual void on_mouse_down(const MouseEvent& event,
                               const ViewTransform& view,
                               NoteCollection& notes,
                               SelectionManager& selection,
                               SnapSystem& snap) {}

    virtual void on_mouse_move(const MouseEvent& event,
                               const ViewTransform& view,
                               NoteCollection& notes,
                               SelectionManager& selection,
                               SnapSystem& snap) {}

    virtual void on_mouse_up(const MouseEvent& event,
                             const ViewTransform& view,
                             NoteCollection& notes,
                             SelectionManager& selection,
                             SnapSystem& snap) {}

    virtual void on_double_click(const MouseEvent& event,
                                 const ViewTransform& view,
                                 NoteCollection& notes,
                                 SelectionManager& selection,
                                 SnapSystem& snap) {}

    virtual Cursor cursor() const { return Cursor::Default; }

    /// Called when this tool becomes active.
    virtual void on_activate() {}

    /// Called when another tool is about to become active.
    virtual void on_deactivate() {}
};

// ─── ToolManager ────────────────────────────────────────────────

/// Manages the active tool and delegates events to the appropriate
/// ToolHandler implementation.
class ToolManager {
public:
    ToolManager();

    void set_active(Tool tool);
    Tool active() const { return active_; }
    Cursor cursor() const;

    // ─── Event delegation ───────────────────────────────────

    void on_mouse_down(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap);

    void on_mouse_move(const MouseEvent& event,
                       const ViewTransform& view,
                       NoteCollection& notes,
                       SelectionManager& selection,
                       SnapSystem& snap);

    void on_mouse_up(const MouseEvent& event,
                     const ViewTransform& view,
                     NoteCollection& notes,
                     SelectionManager& selection,
                     SnapSystem& snap);

    void on_double_click(const MouseEvent& event,
                         const ViewTransform& view,
                         NoteCollection& notes,
                         SelectionManager& selection,
                         SnapSystem& snap);

    // ─── Tool access (for direct use) ───────────────────────

    ToolHandler* handler_for(Tool tool);

private:
    Tool active_ = Tool::Select;
    std::unique_ptr<ToolHandler> handlers_[10];

    void init_handlers();
    size_t tool_index(Tool t) const;
};

} // namespace aria

#endif // ARIA_PIANOROLL_TOOL_MANAGER_H
