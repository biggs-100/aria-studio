# Design: P10c Input System

## Technical Approach

Two new service-manager classes (`FocusManager`, `ShortcutManager`) registered via `ServiceLocator`, plus touch/pen event types and accessibility metadata on the `Widget` base. `FocusManager` uses the widget tree's tab-order list; `ShortcutManager` uses priority-ordered handler dispatch. Both integrate into `RenderLoop`'s input-processing phase. Touch/pen extends `EventDispatcher`'s `EventType` enum and dispatch methods with no structural changes.

Ref: Specs `input-focus-shortcut` and `input-event-extensions`.

## Architecture Decisions

| Decision | Choice | Alternatives | Rationale |
|----------|--------|-------------|-----------|
| Manager pattern | **ServiceLocator registration** | Singleton, injected via constructor | Matches existing pattern (ThemeEngine, RenderLoop use ServiceLocator) |
| Tab order source | **`Widget::tab_index` field** | Dynamic Z-order from tree | Explicit index gives predictable focus; default 0 = insertion order |
| Shortcut priority | **Priority int, focused widget wins** | Tree-walk parent chain | Simpler and faster; avoids walking widget tree for every keystroke |
| Touch/pen dispatch | **New methods on EventDispatcher** | Separate dispatcher class | Minimal diff; shares existing HiDPI scaling and hit-test pipeline |
| Accessibility fields | **Widget header fields** | Separate AccessibleNode class | Keeps data with the widget; subclasses like ButtonWidget set roles naturally |
| Focus ring | **SkiaCanvas draw in FocusManager** | Widget-level paint override | Focus ring must overlay all widgets; manager-level draw avoids per-widget cooperation |

## Data Flow

```
Input event (OS)
    │
    ├─ EventDispatcher dispatches to TS callback
    │   (mouse/key/touch/pen → logical coords)
    │
    └─ RenderLoop input phase:
        ├─ dispatch_key → ShortcutManager.process()
        │   └─ Focused widget handler (highest priority) → propagate on false
        ├─ Tab key → FocusManager.focus_next() / focus_previous()
        │   └─ Dispatch kFocusLost / kFocusGained events
        └─ Paint pass: FocusManager renders focus ring on focused widget
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/graphics/focus_manager.h` | Create | `FocusManager` — tab order list, focus traversal, focus ring render |
| `src/graphics/focus_manager.cc` | Create | Implementation |
| `src/graphics/shortcut_manager.h` | Create | `ShortcutManager` — register/dispatch/conflict detection |
| `src/graphics/shortcut_manager.cc` | Create | Implementation |
| `src/graphics/input_types.h` | Create | `TouchEvent`, `PenEvent` structs; `EventType` touch/pen enum values |
| `src/graphics/ffi/event_dispatcher.h` | Modify | Add `kTouchDown/kTouchMove/kTouchUp/kPenDown/kPenMove/kPenUp` to EventType; add dispatch methods for touch/pen |
| `src/graphics/ffi/event_dispatcher.cc` | Modify | Implement touch/pen dispatch, is_touch_or_pen flag |
| `src/graphics/widget.h` | Modify | Add `tab_index`, `accessible_name`, `accessible_role`, `focus_state` fields; `effective_role()` method |
| `src/graphics/widget.cc` | Modify | Default init new fields |
| `src/graphics/render_loop.cc` | Modify | Call FocusManager/ShortcutManager in input phase; focus ring render after paint |

## Interfaces / Contracts

```cpp
// input_types.h
struct TouchEvent {
    float x, y;
    int   touch_id;
    // inherited: modifiers, timestamp
};
struct PenEvent {
    float x, y;
    float pressure;       // 0.0–1.0
    float tilt_x, tilt_y; // degrees
    bool  barrel_button;
};

// focus_manager.h
class FocusManager {
public:
    void rebuild_list(Widget* root);            // scan widget tree for tab_index
    void focus_next();
    void focus_previous();
    void set_focus(WidgetID id);
    WidgetID focused_widget() const noexcept;
    void render_focus_ring(SkiaCanvas* canvas);  // after paint pass
private:
    std::vector<WidgetID> tab_order_;
    WidgetID focused_ = kInvalidWidgetID;
};

// shortcut_manager.h
using ShortcutHandler = std::function<bool()>;  // return true if consumed
struct KeyChord { int key; bool ctrl, shift, alt, meta; };

class ShortcutManager {
public:
    void register_shortcut(KeyChord chord, ShortcutHandler handler, int priority = 0);
    bool dispatch(KeyChord chord);               // returns true if consumed
    std::vector<ShortcutHandler> conflicts(KeyChord chord) const;
};

// widget.h additions
enum class AccessibleRole { kNone, kButton, kSlider, kLabel, kPanel, kList, kTextInput, kTabStop };
struct Widget {
    int tab_index_ = 0;
    std::string accessible_name_;
    AccessibleRole accessible_role_ = AccessibleRole::kNone;
    std::string accessible_description_;
    AccessibleRole effective_role() const;  // inherits parent's role if kNone
};
```

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Unit | FocusManager tab order traversal | Build 3-widget tree with tab_index 1,2,3; verify focus_next() cycles correctly |
| Unit | FocusManager wrap-around | Verify focus_next() at end of list wraps to first widget |
| Unit | ShortcutManager dispatch priority | Register same chord at priorities 10 and 5; verify 10 fires first |
| Unit | ShortcutManager propagation | Focused handler returns false → verify next handler fires |
| Unit | ShortcutManager conflict warning | Register duplicate chord, verify warning logged |
| Unit | TouchEvent dispatch through EventDispatcher | Verify hit-test and coordinate conversion |
| Unit | Widget effective_role inheritance | Parent=kPanel, child=kNone → effective_role returns kPanel |
| Integration | RenderLoop keyboard → ShortcutManager | Dispatch key down, verify shortcut handler invoked |

## Migration / Rollout

No migration. `EventDispatcher` dispatch methods are additive. `Widget` base fields have safe defaults. Existing event handlers are unaffected.

## Open Questions

- [ ] Focus ring: draw as SkiaCanvas overlay or as a thematic RectWidget extension? (Overlay in FocusManager keeps it focused-widget-specific, simpler for now.)
