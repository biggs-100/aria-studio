#include "graphics/widget_themed.h"

#include <algorithm>

namespace aria {

// ── Static registry ────────────────────────────────────────────────

std::vector<ThemedWidget*>* ThemedWidget::registry() {
    static std::vector<ThemedWidget*>* reg = new std::vector<ThemedWidget*>();
    return reg;
}

void ThemedWidget::register_widget(ThemedWidget* widget) {
    auto* reg = registry();
    if (std::find(reg->begin(), reg->end(), widget) == reg->end()) {
        reg->push_back(widget);
    }
}

void ThemedWidget::unregister_widget(ThemedWidget* widget) {
    auto* reg = registry();
    auto it = std::find(reg->begin(), reg->end(), widget);
    if (it != reg->end()) {
        reg->erase(it);
    }
}

void ThemedWidget::notify_all() {
    auto* reg = registry();
    for (auto* widget : *reg) {
        widget->onThemeChanged();
    }
}

// ── Construction / Destruction ─────────────────────────────────────

ThemedWidget::ThemedWidget() {
    // Auto-register for theme change notifications.
    // The widget is unregistered in the destructor.
    register_widget(this);
}

ThemedWidget::~ThemedWidget() {
    unregister_widget(this);
}

// ── Theme change handler ───────────────────────────────────────────

void ThemedWidget::onThemeChanged() {
    mark_dirty();
}

} // namespace aria
