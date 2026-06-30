#ifndef ARIA_LAYOUT_ENGINE_H
#define ARIA_LAYOUT_ENGINE_H

#include "graphics/graphics_types.h"

namespace aria {

class Widget;

/// Two-pass layout engine: measure → arrange.
///
/// Traverses the widget tree depth-first, first computing preferred
/// sizes (measure pass), then assigning final bounds (arrange pass).
///
/// Design decisions (from design.md):
///   - Two-phase: measure determines preferred size; arrange assigns
///     final bounds within the viewport.
///   - Flex layout: children with flex factors distribute remaining space.
class LayoutEngine {
public:
    LayoutEngine() = default;
    ~LayoutEngine() = default;

    /// Run the full layout cycle on a widget tree.
    /// @param root  The root widget to layout. If null, no-op.
    /// @param viewport  The available viewport size.
    void compute(Widget* root, Vec2 viewport);

private:
    void measure_pass(Widget* widget, float available_w, float available_h);
    void arrange_pass(Widget* widget, float x, float y, float w, float h);
};

} // namespace aria

#endif // ARIA_LAYOUT_ENGINE_H
