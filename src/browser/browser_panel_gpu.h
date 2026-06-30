#ifndef ARIA_BROWSER_PANEL_GPU_H
#define ARIA_BROWSER_PANEL_GPU_H

#include "graphics/graphics_types.h"
#include "graphics/widget_container.h"
#include "graphics/widget_text.h"
#include "graphics/widget_rect.h"
#include "graphics/widget_button.h"
#include "graphics/widget_themed.h"
#include "browser/waveform_cache.h"
#include "browser/browser_engine.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace aria {

// Forward declarations
class ProjectManager;

namespace browser {

// ── Forward declarations ──────────────────────────────────────────

/// Waveform data from WaveformCache.
using WaveformData = WaveformCache::WaveformData;

// ── TreeNodeWidget ────────────────────────────────────────────────

/// A tree node widget for the browser folder/category tree.
///
/// Renders a clickable row with indentation based on depth.
/// Supports expand/collapse toggle. Used as child items inside
/// a scrollable ContainerWidget.
class TreeNodeWidget : public Widget {
public:
    TreeNodeWidget();
    ~TreeNodeWidget() override;

    const char* type_name() const noexcept override { return "TreeNodeWidget"; }

    // ── Label ──────────────────────────────────────────────────

    void set_label(std::string_view text);
    const std::string& label() const noexcept;

    // ── Tree state ─────────────────────────────────────────────

    void set_expanded(bool expanded) noexcept;
    bool is_expanded() const noexcept { return expanded_; }

    void set_depth(int depth) noexcept { depth_ = depth; mark_dirty(); }
    int depth() const noexcept { return depth_; }

    // ── Layout ─────────────────────────────────────────────────

    Vec2 measure(float available_width, float available_height) override;

    // ── Paint ──────────────────────────────────────────────────

    void render(SkiaCanvas* canvas) override;

private:
    std::string label_;
    bool expanded_ = false;
    int depth_ = 0;  ///< Nesting level (0 = root)
};

// ── WaveformPreviewWidget ─────────────────────────────────────────

/// Renders a waveform thumbnail using SkiaCanvas drawLine calls.
///
/// Accepts WaveformData from WaveformCache and renders peak/min
/// lines during the paint pass. Shows "No waveform data" text
/// when no data is available.
///
/// PR 4 polish: time ruler ticks rendered at bottom, zoom level
/// indicator shown at top-right corner, zoom level clamped to
/// [0.1, 100.0].
class WaveformPreviewWidget : public Widget {
public:
    WaveformPreviewWidget();
    ~WaveformPreviewWidget() override;

    const char* type_name() const noexcept override { return "WaveformPreviewWidget"; }

    // ── Waveform data ─────────────────────────────────────────

    void set_waveform(const WaveformData& data);
    void clear_waveform();
    bool has_waveform() const noexcept;
    const WaveformData& waveform_data() const noexcept;

    // ── Layout ─────────────────────────────────────────────────

    Vec2 measure(float available_width, float available_height) override;

    // ── Paint ──────────────────────────────────────────────────

    void render(SkiaCanvas* canvas) override;

    // ── Zoom level (PR 4) ──────────────────────────────────────

    /// Set zoom level, clamped to [0.1, 100.0]. Returns true if changed.
    bool set_zoom_level(float zoom);
    float zoom_level() const noexcept { return zoom_level_; }

    /// Human-readable zoom label (e.g. "2.5x").
    const char* zoom_label() const noexcept;

    // ── Time ruler (PR 4) ──────────────────────────────────────

    /// Show or hide the time ruler ticks at the bottom of the preview.
    void set_time_ruler_visible(bool visible) noexcept { time_ruler_visible_ = visible; mark_dirty(); }
    bool time_ruler_visible() const noexcept { return time_ruler_visible_; }

private:
    std::optional<WaveformData> waveform_;
    Color waveform_color_ = Color::from_rgba8(100, 200, 255);   // fallback — resolved from theme at render time
    Color bg_color_       = Color::from_rgba8(30, 30, 30);     // resolved from "colors.waveform.bg"
    Color text_color_     = Color::from_rgba8(128, 128, 128);  // resolved from "colors.text.secondary"
    Color ruler_color_    = Color::from_rgba8(70, 70, 80);     // resolved from "colors.waveform.ruler"
    Color ruler_tick_color_ = Color::from_rgba8(100, 100, 110); // resolved from "colors.waveform.tick"

    // PR 4 state
    float zoom_level_ = 1.0f;
    char  zoom_label_buf_[16] = {};
    bool  time_ruler_visible_ = true;

    void render_waveform(SkiaCanvas* canvas);
    void render_fallback(SkiaCanvas* canvas);
    void render_time_ruler(SkiaCanvas* canvas);
    void render_zoom_indicator(SkiaCanvas* canvas);
};

// ── BrowserPanelGPU ───────────────────────────────────────────────

/// GPU-rendered browser panel as a ContainerWidget tree.
///
/// Replaces the provisional ImGui BrowserPanel with a fully GPU-
/// native widget hierarchy: toolbar with view mode buttons, search
/// bar, scrollable tree/results list, and waveform preview panel.
///
/// Design:
///   - Root is a ContainerWidget with vertical layout.
///   - Children: toolbar ContainerWidget, tree ContainerWidget,
///     preview WaveformPreviewWidget.
///   - View mode toggles between FolderTree, CategoryTree, SearchResults.
///   - Search triggers BrowserEngine::search()->search_sync().
///
/// PR 4 DnD: file drag callback + ProjectManager integration for
/// creating audio clips from dragged files.
class BrowserPanelGPU : public ContainerWidget {
public:
    /// View modes matching the provisional BrowserPanel::ViewMode.
    enum class ViewMode {
        FolderTree,
        CategoryTree,
        SearchResults
    };

    /// Callback fired when a file drag is initiated from the GPU panel.
    using FileDragCallback = std::function<void(const std::string& filepath)>;

    explicit BrowserPanelGPU(BrowserEngine* engine = nullptr);
    ~BrowserPanelGPU() override;

    const char* type_name() const noexcept override { return "BrowserPanelGPU"; }

    // ── View mode ──────────────────────────────────────────────

    void set_view_mode(ViewMode mode);
    ViewMode view_mode() const noexcept { return view_mode_; }

    // ── Search ─────────────────────────────────────────────────

    void set_search_text(std::string_view text);
    const std::string& search_text() const noexcept;

    /// Execute the current search query via BrowserEngine.
    void execute_search();

    // ── Render ─────────────────────────────────────────────────

    void render(SkiaCanvas* canvas) override;

    // ── Drag-and-drop (PR 4) ───────────────────────────────────

    /// Register a handler for file drag events.
    void on_file_drag(FileDragCallback cb) { drag_cb_ = std::move(cb); }

    /// Initiate a file drag from the GPU panel (called from widget tree
    /// on drag gesture detection). Fires the registered callback.
    void begin_file_drag(const std::string& filepath);

    /// Set the ProjectManager for direct clip creation.
    void set_project_manager(ProjectManager* pm) noexcept { pm_ = pm; }
    ProjectManager* project_manager() const noexcept { return pm_; }

private:
    BrowserEngine* engine_ = nullptr;
    ProjectManager* pm_ = nullptr;
    ViewMode view_mode_ = ViewMode::FolderTree;
    std::string search_text_;

    // Drag-and-drop callback
    FileDragCallback drag_cb_;

    // Stored waveform data for preview rebuild
    std::optional<WaveformData> waveform_data_;
    bool has_waveform() const noexcept {
        return waveform_data_.has_value() && !waveform_data_->peaks.empty();
    }
    void set_waveform_data(const WaveformData& data) { waveform_data_ = data; }

    // Child widget pointers (owned by ContainerWidget::children_)
    Widget* search_bar_ = nullptr;
    ContainerWidget* tree_view_ = nullptr;
    WaveformPreviewWidget* waveform_preview_ = nullptr;

    void rebuild_children();
};

} // namespace browser
} // namespace aria

#endif // ARIA_BROWSER_PANEL_GPU_H
