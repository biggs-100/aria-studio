#include "browser/browser_panel_gpu.h"
#include "graphics/skia_canvas.h"
#include "graphics/theme_engine.h"
#include "browser/browser_engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unordered_map>

namespace aria {
namespace browser {

// ── Theme color helper ───────────────────────────────────────
static Color theme_color(const char* path, Color fallback) {
    auto* engine = ThemeEngine::get();
    if (!engine) return fallback;
    return engine->resolve_color(path, fallback);
}

// ═══════════════════════════════════════════════════════════════
// TreeNodeWidget
// ═══════════════════════════════════════════════════════════════

TreeNodeWidget::TreeNodeWidget() = default;
TreeNodeWidget::~TreeNodeWidget() = default;

void TreeNodeWidget::set_label(std::string_view text) {
    label_ = text;
    mark_dirty();
}

const std::string& TreeNodeWidget::label() const noexcept {
    return label_;
}

void TreeNodeWidget::set_expanded(bool expanded) noexcept {
    if (expanded_ != expanded) {
        expanded_ = expanded;
        mark_dirty();
    }
}

Vec2 TreeNodeWidget::measure(float /*available_width*/, float /*available_height*/) {
    Vec2 result = {0, 24.0f};
    set_preferred_size(result);
    return result;
}

void TreeNodeWidget::render(SkiaCanvas* canvas) {
    if (!canvas || !is_visible() || label_.empty()) return;

    // ── Background ─────────────────────────────────────────────
    Paint bg;
    bg.fill = is_hovered()
        ? theme_color("colors.bg.hover", Color::from_rgba8(50, 50, 60))
        : theme_color("colors.bg.secondary", Color::from_rgba8(35, 35, 40));
    canvas->drawRect(bounds(), bg);

    // ── Indentation ────────────────────────────────────────────
    float indent = static_cast<float>(depth_) * 16.0f + 4.0f;

    // ── Expand/collapse arrow ──────────────────────────────────
    float arrow_x = bounds().x + indent;
    float arrow_y = bounds().y + (bounds().h - 10.0f) * 0.5f;
    Paint arrow_paint;
    arrow_paint.fill = theme_color("colors.text.secondary", Color::from_rgba8(180, 180, 180));
    canvas->drawRect({arrow_x, arrow_y, 10.0f, 10.0f}, arrow_paint);

    // ── Label text ─────────────────────────────────────────────
    float text_x = bounds().x + indent + 16.0f;
    float text_y = bounds().y + (bounds().h - 14.0f) * 0.5f;
    Font font;
    font.size = 14.0f;
    Paint text_paint;
    text_paint.fill = theme_color("colors.text.primary", Color::from_rgba8(220, 220, 220));
    canvas->drawTextBlob(label_.c_str(), font, text_x, text_y, text_paint);
}

// ═══════════════════════════════════════════════════════════════
// WaveformPreviewWidget
// ═══════════════════════════════════════════════════════════════

WaveformPreviewWidget::WaveformPreviewWidget() {
    std::snprintf(zoom_label_buf_, sizeof(zoom_label_buf_), "1.0x");
}
WaveformPreviewWidget::~WaveformPreviewWidget() = default;

void WaveformPreviewWidget::set_waveform(const WaveformData& data) {
    waveform_ = data;
    mark_dirty();
}

void WaveformPreviewWidget::clear_waveform() {
    waveform_.reset();
    mark_dirty();
}

bool WaveformPreviewWidget::has_waveform() const noexcept {
    return waveform_.has_value() && !waveform_->peaks.empty();
}

const WaveformData& WaveformPreviewWidget::waveform_data() const noexcept {
    static const WaveformData empty{};
    return waveform_.has_value() ? *waveform_ : empty;
}

Vec2 WaveformPreviewWidget::measure(float /*available_width*/, float /*available_height*/) {
    Vec2 result = {0, 100.0f};
    set_preferred_size(result);
    return result;
}

bool WaveformPreviewWidget::set_zoom_level(float zoom) {
    float clamped = std::clamp(zoom, 0.1f, 100.0f);
    if (zoom_level_ == clamped) return false;
    zoom_level_ = clamped;
    std::snprintf(zoom_label_buf_, sizeof(zoom_label_buf_), "%.1fx", zoom_level_);
    mark_dirty();
    return true;
}

const char* WaveformPreviewWidget::zoom_label() const noexcept {
    return zoom_label_buf_;
}

void WaveformPreviewWidget::render(SkiaCanvas* canvas) {
    if (!canvas || !is_visible()) return;

    // ── Background ─────────────────────────────────────────────
    Paint bg;
    bg.fill = bg_color_;
    canvas->drawRect(bounds(), bg);

    // Content area (reserve 14px at bottom for time ruler if visible)
    float ruler_h = time_ruler_visible_ ? 14.0f : 0.0f;
    float old_h = bounds().h;
    // Temporarily shrink bounds for waveform rendering
    Rect content_bounds = bounds();
    content_bounds.h -= ruler_h;

    // Save/restore canvas state
    canvas->save();
    canvas->clipRect(content_bounds);

    if (has_waveform()) {
        render_waveform(canvas);
    } else {
        render_fallback(canvas);
    }

    canvas->restore();

    // Draw time ruler and zoom indicator on top
    if (time_ruler_visible_) {
        render_time_ruler(canvas);
    }
    render_zoom_indicator(canvas);
}

void WaveformPreviewWidget::render_waveform(SkiaCanvas* canvas) {
    const auto& wf = *waveform_;
    size_t num_samples = wf.peaks.size();
    if (num_samples == 0) return;

    float content_x = bounds().x + 2.0f;
    float content_y = bounds().y + 2.0f;
    float content_w = bounds().w - 4.0f;
    float ruler_h = time_ruler_visible_ ? 14.0f : 0.0f;
    float content_h = bounds().h - 4.0f - ruler_h;
    float center_y = content_y + content_h * 0.5f;
    float step = content_w / static_cast<float>(num_samples);

    Paint line_paint;
    line_paint.fill  = waveform_color_;
    line_paint.stroke = waveform_color_;
    line_paint.stroke_width = 1.0f;

    for (size_t i = 0; i < num_samples; ++i) {
        float x = content_x + static_cast<float>(i) * step;
        float peak_h = (wf.peaks[i] * 0.5f + 0.5f) * content_h * 0.5f;
        float min_h  = (wf.minima[i] * 0.5f + 0.5f) * content_h * 0.5f;

        // Draw vertical line from peak to minima
        // Using drawRect for thin vertical line (1px wide)
        canvas->drawRect(
            {x, center_y - peak_h, std::max(step, 1.0f), peak_h + min_h},
            line_paint
        );
    }
}

void WaveformPreviewWidget::render_fallback(SkiaCanvas* canvas) {
    float ruler_h = time_ruler_visible_ ? 14.0f : 0.0f;
    float content_h = bounds().h - 4.0f - ruler_h;

    // Center line
    Paint line_paint;
    line_paint.stroke = theme_color("colors.waveform.grid", Color::from_rgba8(80, 80, 80));
    line_paint.stroke_width = 1.0f;
    float center_y = bounds().y + 2.0f + content_h * 0.5f;
    canvas->drawRect(
        {bounds().x + 2.0f, center_y, bounds().w - 4.0f, 1.0f},
        line_paint
    );

    // "No waveform data" text
    Font font;
    font.size = 14.0f;
    Paint text_paint;
    text_paint.fill = text_color_;
    canvas->drawTextBlob("No waveform data", font,
                         bounds().x + 10.0f, center_y - 7.0f,
                         text_paint);
}

void WaveformPreviewWidget::render_time_ruler(SkiaCanvas* canvas) {
    float ruler_y = bounds().y + bounds().h - 14.0f;
    float ruler_w = bounds().w;

    // Background strip
    Paint ruler_bg;
    ruler_bg.fill = ruler_color_;
    canvas->drawRect({bounds().x, ruler_y, ruler_w, 14.0f}, ruler_bg);

    // Tick marks — draw 5 evenly spaced ticks
    // Each tick represents a time division
    int num_ticks = 5;
    float tick_spacing = (ruler_w - 8.0f) / static_cast<float>(num_ticks - 1);

    Paint tick_paint;
    tick_paint.fill = ruler_tick_color_;

    Font tick_font;
    tick_font.size = 9.0f;
    Paint tick_text_paint;
    tick_text_paint.fill = theme_color("colors.text.secondary", Color::from_rgba8(160, 160, 170));

    for (int i = 0; i < num_ticks; ++i) {
        float tx = bounds().x + 4.0f + static_cast<float>(i) * tick_spacing;

        // Tick line (small vertical mark)
        canvas->drawRect({tx, ruler_y, 1.0f, 4.0f}, tick_paint);

        // Tick label (seconds placeholder)
        char label[8];
        std::snprintf(label, sizeof(label), "%ds", i);
        canvas->drawTextBlob(label, tick_font, tx - 4.0f, ruler_y + 12.0f, tick_text_paint);
    }
}

void WaveformPreviewWidget::render_zoom_indicator(SkiaCanvas* canvas) {
    // Draw zoom level in top-right corner
    Font zoom_font;
    zoom_font.size = 10.0f;
    Paint zoom_bg;
    zoom_bg.fill = theme_color("colors.overlay.tooltip", Color::from_rgba8(0, 0, 0, 140));
    zoom_bg.corner_radius = 3.0f;

    // Approximate text width for background pill
    float text_width = 40.0f;  // wide enough for "100.0x"
    float pill_x = bounds().x + bounds().w - text_width - 6.0f;
    float pill_y = bounds().y + 4.0f;

    canvas->drawRect({pill_x, pill_y, text_width, 14.0f}, zoom_bg);

    Paint zoom_text_paint;
    zoom_text_paint.fill = theme_color("colors.text.secondary", Color::from_rgba8(200, 200, 210));
    canvas->drawTextBlob(zoom_label_buf_, zoom_font,
                         pill_x + 3.0f, pill_y + 11.0f,
                         zoom_text_paint);
}

// ═══════════════════════════════════════════════════════════════
// BrowserPanelGPU
// ═══════════════════════════════════════════════════════════════

BrowserPanelGPU::BrowserPanelGPU(BrowserEngine* engine)
    : engine_(engine) {}

BrowserPanelGPU::~BrowserPanelGPU() = default;

void BrowserPanelGPU::set_view_mode(ViewMode mode) {
    if (view_mode_ != mode) {
        view_mode_ = mode;
        mark_dirty();
    }
}

void BrowserPanelGPU::set_search_text(std::string_view text) {
    search_text_ = text;
    mark_dirty();
}

const std::string& BrowserPanelGPU::search_text() const noexcept {
    return search_text_;
}

void BrowserPanelGPU::execute_search() {
    if (!engine_ || search_text_.empty()) return;

    SearchParams params;
    params.text = search_text_;
    params.limit = 200;

    auto results = engine_->search()->search_sync(params);

    // Results are consumed by the tree view — for now just mark dirty
    mark_dirty();
}

void BrowserPanelGPU::render(SkiaCanvas* canvas) {
    if (!canvas || !is_visible()) return;

    // Delegate to ContainerWidget's render for clip/transform
    // then render custom background and toolbar indicators
    Paint bg;
    bg.fill = theme_color("colors.bg.primary", Color::from_rgba8(25, 25, 30));
    canvas->drawRect(bounds(), bg);

    // Render children (toolbar, tree, preview)
    render_children(canvas);
}

void BrowserPanelGPU::rebuild_children() {
    // Clear existing children
    children_.clear();

    // ── Search bar ─────────────────────────────────────────────
    auto search_bar = std::make_unique<RectWidget>();
    search_bar->set_bounds({bounds().x, bounds().y, bounds().w, 36.0f});
    search_bar_ = search_bar.get();
    add_child(std::move(search_bar));

    // ── Tree view ──────────────────────────────────────────────
    float tree_top = bounds().y + 36.0f;
    float preview_h = has_waveform() ? 100.0f : 0.0f;
    float tree_h = bounds().h - 36.0f - preview_h;

    auto tree = std::make_unique<ContainerWidget>();
    tree->set_bounds({bounds().x, tree_top, bounds().w, tree_h});
    tree_view_ = tree.get();
    add_child(std::move(tree));

    // Populate tree with category nodes from search results
    if (engine_ && !search_text_.empty()) {
        SearchParams params;
        params.text = search_text_;
        params.limit = 200;
        auto results = engine_->search()->search_sync(params);

        // Group results by category into tree nodes
        std::unordered_map<std::string, std::vector<SampleResult>> by_category;
        for (auto& r : results) {
            by_category[r.sample.category.value_or("Uncategorized")].push_back(r);
        }

        for (auto& [cat, samples] : by_category) {
            auto cat_node = std::make_unique<TreeNodeWidget>();
            cat_node->set_label(cat);
            cat_node->set_depth(0);
            cat_node->set_expanded(true);

            for (auto& s : samples) {
                auto file_node = std::make_unique<TreeNodeWidget>();
                file_node->set_label(s.sample.name.value_or(s.sample.file_path));
                file_node->set_depth(1);
                cat_node->add_child(std::move(file_node));
            }
            tree_view_->add_child(std::move(cat_node));
        }
    }

    // ── Waveform preview ───────────────────────────────────────
    float preview_y = bounds().y + bounds().h - preview_h;
    auto wf_preview = std::make_unique<WaveformPreviewWidget>();
    wf_preview->set_bounds({bounds().x, preview_y, bounds().w, preview_h});
    if (waveform_data_) {
        wf_preview->set_waveform(*waveform_data_);
    }
    waveform_preview_ = wf_preview.get();
    add_child(std::move(wf_preview));

    mark_dirty();
}

void BrowserPanelGPU::begin_file_drag(const std::string& filepath) {
    // Fire the registered drag callback. The BrowserPanel listens on this
    // and routes to ProjectManager::create_audio_clip() for clip creation.
    if (drag_cb_) {
        drag_cb_(filepath);
    }
}

} // namespace browser
} // namespace aria
