#pragma once

#include "kernel/event_types.h"
#include "browser/arrangement_drop_target.h"
#include "browser/browser_engine.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "aria_rust.h"

namespace aria::browser { class BrowserPanelGPU; }
#ifdef ARIA_FEATURE_GPU
#include "browser/browser_panel_gpu.h"
#endif

namespace aria {
namespace browser {

/// Browser panel with GPU rendering support.
///
/// When ARIA_FEATURE_GPU is enabled, the panel renders as a
/// ContainerWidget tree (BrowserPanelGPU) via Dawn/Skia.
/// Otherwise, it falls back to lightweight stubs.
///
/// Provides folder tree, category tree, search bar with results,
/// preview panel (waveform thumbnail + metadata), and drag source.
class BrowserPanel {
public:
    explicit BrowserPanel(BrowserEngine* engine,
                          ProjectManager* pm = nullptr);
    ~BrowserPanel() = default;

    // Non-copyable
    BrowserPanel(const BrowserPanel&) = delete;
    BrowserPanel& operator=(const BrowserPanel&) = delete;

    // ─── View modes ───────────────────────────────────────────

    enum class ViewMode {
        FolderTree,
        CategoryTree,
        SearchResults
    };

    void set_view_mode(ViewMode mode);
    ViewMode view_mode() const { return view_mode_; }

    // ─── Main draw call ───────────────────────────────────────

    /// Draw the full browser panel.
    /// When GPU is enabled, renders via BrowserPanelGPU → SkiaCanvas.
    /// Otherwise, uses lightweight stubs.
    void draw();

    // ─── Selection ────────────────────────────────────────────

    /// Get the currently selected file path.
    const std::string& selected_file() const { return selected_file_; }

    /// Get all selected file paths.
    const std::vector<std::string>& selected_files() const { return selected_files_; }

    // ─── Drag-and-drop ────────────────────────────────────────

    /// Callback fired when a file is dragged from the browser.
    using FileDragCallback = std::function<void(const std::string& path)>;

    /// Register a handler for file drag operations.
    void on_file_drag(FileDragCallback cb) { drag_cb_ = std::move(cb); }

    // ─── Events ───────────────────────────────────────────────

    /// Callback fired when the selected file changes.
    using SelectionCallback = std::function<void(const std::string& path)>;
    void on_selection_change(SelectionCallback cb) { sel_cb_ = std::move(cb); }

    /// Access the GPU panel (when available).
    BrowserPanelGPU* gpu_panel() {
#ifdef ARIA_FEATURE_GPU
        return gpu_panel_.get();
#else
        return nullptr;
#endif
    }

private:
    // ─── Draw helpers (pre-GPU fallback) ──────────────────────

    void draw_toolbar();
    void draw_search_bar();
    void draw_folder_tree();
    void draw_category_tree();
    void draw_search_results();
    void draw_preview_panel();

    /// Build a category tree from the database.
    struct CategoryNode {
        std::string name;
        int32_t     count = 0;
        std::vector<CategoryNode> children;
    };
    void build_category_tree(std::vector<CategoryNode>& nodes);

    /// Initiate a search with the current query text.
    void execute_search();

    // ─── Arrangement DnD ──────────────────────────────────────

    /// Access the provisional arrangement drop target.
    ArrangementDropTarget* arrangement_drop() { return arrangement_drop_.get(); }

    // ─── State ────────────────────────────────────────────────

    BrowserEngine* engine_;
    ViewMode       view_mode_ = ViewMode::FolderTree;

    // Search
    char            search_buffer_[256] = {};
    SearchResult    current_results_;
    bool            results_dirty_ = false;

    // Selection
    std::string              selected_file_;
    std::vector<std::string> selected_files_;

    // Provisional arrangement drop target (P10 replaces with GPU view)
    std::unique_ptr<ArrangementDropTarget> arrangement_drop_;

    // Callbacks
    FileDragCallback    drag_cb_;
    SelectionCallback   sel_cb_;

    // Cached category tree
    std::vector<CategoryNode> category_nodes_;
    bool                       category_tree_dirty_ = true;

    // GPU widget panel (when ARIA_FEATURE_GPU is enabled)
#ifdef ARIA_FEATURE_GPU
    std::unique_ptr<BrowserPanelGPU> gpu_panel_;
#endif
};

} // namespace browser
} // namespace aria
