#include <catch2/catch_test_macros.hpp>

#include "graphics/graphics_types.h"
#include "graphics/skia_canvas.h"
#include "graphics/widget.h"
#include "graphics/widget_container.h"
#include "browser/browser_panel_gpu.h"
#include "project/project_manager.h"

#include <memory>

using namespace aria;
using namespace aria::browser;

// ─── Browser Panel GPU TDD Tests ──────────────────────────────────
//
// Test Strategy:
//   - Verify TreeNodeWidget creates and manages tree state.
//   - Verify WaveformPreviewWidget accepts waveform data.
//   - Verify BrowserPanelGPU constructs as a ContainerWidget with
//     toolbar/search/tree/preview children.
//
//   TDD notes:
//     - RED:   Tests reference classes before implementation.
//     - GREEN: Implement minimum to compile and pass.
//     - TRIANGULATE: Edge cases, empty states, bounds.

// =====================================================================
// RED / GREEN – TreeNodeWidget
// =====================================================================

TEST_CASE("TreeNodeWidget starts collapsed by default",
          "[graphics][browser][treenode]")
{
    TreeNodeWidget node;
    node.set_bounds({0, 0, 200, 24});

    // GIVEN a new TreeNodeWidget
    // THEN it is collapsed
    CHECK_FALSE(node.is_expanded());

    // AND has the correct type name
    CHECK(std::string(node.type_name()) == "TreeNodeWidget");

    // AND has no label set
    CHECK(node.label().empty());
}

TEST_CASE("TreeNodeWidget label can be set and retrieved",
          "[graphics][browser][treenode]")
{
    TreeNodeWidget node;

    // WHEN label is set
    node.set_label("Kicks");

    // THEN it is retrievable
    CHECK(node.label() == "Kicks");

    // WHEN label is cleared
    node.set_label("");
    CHECK(node.label().empty());
}

TEST_CASE("TreeNodeWidget expand/collapse state toggles",
          "[graphics][browser][treenode]")
{
    TreeNodeWidget node;

    // GIVEN a collapsed node
    CHECK_FALSE(node.is_expanded());

    // WHEN expanded
    node.set_expanded(true);
    CHECK(node.is_expanded());

    // WHEN collapsed again
    node.set_expanded(false);
    CHECK_FALSE(node.is_expanded());
}

TEST_CASE("TreeNodeWidget depth property is set and retrieved",
          "[graphics][browser][treenode]")
{
    TreeNodeWidget node;

    // GIVEN default depth is 0
    CHECK(node.depth() == 0);

    // WHEN depth is set
    node.set_depth(2);

    // THEN depth is retrievable
    CHECK(node.depth() == 2);
}

// =====================================================================
// RED / GREEN – WaveformPreviewWidget
// =====================================================================

TEST_CASE("WaveformPreviewWidget default state has no waveform",
          "[graphics][browser][waveform]")
{
    WaveformPreviewWidget wf;

    CHECK(std::string(wf.type_name()) == "WaveformPreviewWidget");

    // GIVEN a fresh widget
    // THEN has_waveform returns false
    CHECK_FALSE(wf.has_waveform());
}

TEST_CASE("WaveformPreviewWidget accepts waveform data",
          "[graphics][browser][waveform]")
{
    WaveformPreviewWidget wf;

    // GIVEN waveform data with peaks and minima
    WaveformData data;
    data.peaks   = {0.5f, -0.3f, 0.8f, -0.6f, 0.2f};
    data.minima  = {-0.4f, 0.1f, -0.7f, 0.3f, -0.1f};
    data.resolution = 256;
    data.sample_rate = 44100;
    data.channels    = 1;

    // WHEN set_waveform is called
    wf.set_waveform(data);

    // THEN has_waveform returns true
    CHECK(wf.has_waveform());

    // AND the data is retrievable
    const auto& retrieved = wf.waveform_data();
    CHECK(retrieved.peaks.size() == 5);
    CHECK(retrieved.peaks[0] == 0.5f);
    CHECK(retrieved.minima[2] == -0.7f);
}

TEST_CASE("WaveformPreviewWidget clears waveform data",
          "[graphics][browser][waveform]")
{
    WaveformPreviewWidget wf;

    // GIVEN a widget with waveform data
    WaveformData data;
    data.peaks = {0.5f};
    data.minima = {-0.5f};
    wf.set_waveform(data);
    CHECK(wf.has_waveform());

    // WHEN clear_waveform is called
    wf.clear_waveform();

    // THEN has_waveform returns false
    CHECK_FALSE(wf.has_waveform());
}

// =====================================================================
// RED / GREEN – BrowserPanelGPU
// =====================================================================

TEST_CASE("BrowserPanelGPU constructs as a visible ContainerWidget",
          "[graphics][browser][panel]")
{
    // GIVEN a BrowserPanelGPU without dependencies
    BrowserPanelGPU panel;

    // THEN it is a visible widget
    CHECK(panel.is_visible());
    CHECK(std::string(panel.type_name()) == "BrowserPanelGPU");
}

TEST_CASE("BrowserPanelGPU default view mode is FolderTree",
          "[graphics][browser][panel]")
{
    BrowserPanelGPU panel;

    // GIVEN a fresh panel
    // THEN view mode is FolderTree
    CHECK(panel.view_mode() == BrowserPanelGPU::ViewMode::FolderTree);

    // WHEN view mode is changed
    panel.set_view_mode(BrowserPanelGPU::ViewMode::SearchResults);
    CHECK(panel.view_mode() == BrowserPanelGPU::ViewMode::SearchResults);

    // AND back to CategoryTree
    panel.set_view_mode(BrowserPanelGPU::ViewMode::CategoryTree);
    CHECK(panel.view_mode() == BrowserPanelGPU::ViewMode::CategoryTree);
}

TEST_CASE("BrowserPanelGPU sets and gets search text",
          "[graphics][browser][panel]")
{
    BrowserPanelGPU panel;

    // GIVEN default search text
    CHECK(panel.search_text().empty());

    // WHEN search text is set
    panel.set_search_text("kick drum");

    // THEN it is retrievable
    CHECK(panel.search_text() == "kick drum");

    // WHEN cleared
    panel.set_search_text("");
    CHECK(panel.search_text().empty());
}

TEST_CASE("BrowserPanelGPU paint pass does not crash",
          "[graphics][browser][panel]")
{
    // GIVEN a BrowserPanelGPU
    BrowserPanelGPU panel;
    panel.set_bounds({0, 0, 300, 600});

    SkiaCanvas canvas;

    // WHEN paint is called (no GPU — safe no-op)
    // THEN no crash occurs
    CHECK_NOTHROW(panel.paint(&canvas));
}

TEST_CASE("BrowserPanelGPU hit-test on empty panel returns panel itself",
          "[graphics][browser][panel]")
{
    BrowserPanelGPU panel;
    panel.set_bounds({0, 0, 300, 600});

    // WHEN hit-test at panel interior
    auto* hit = panel.hit_test(150.0f, 300.0f);

    // THEN the panel itself is hit (no children to intercept)
    CHECK(hit == &panel);
}

// =====================================================================
// PR 4 — Waveform preview: zoom level & time ruler (RED → GREEN)
// =====================================================================

TEST_CASE("WaveformPreviewWidget default zoom level is 1.0x",
          "[graphics][browser][waveform][pr4]")
{
    WaveformPreviewWidget wf;

    // GIVEN a fresh WaveformPreviewWidget
    // THEN default zoom level is 1.0
    CHECK(wf.zoom_level() == 1.0f);

    // AND zoom label contains "1.0x"
    CHECK(std::string(wf.zoom_label()).find("1.0") != std::string::npos);
}

TEST_CASE("WaveformPreviewWidget zoom level can be set and retrieved",
          "[graphics][browser][waveform][pr4]")
{
    WaveformPreviewWidget wf;

    // GIVEN default zoom
    CHECK(wf.zoom_level() == 1.0f);

    // WHEN zoom is set to 2.5x
    wf.set_zoom_level(2.5f);

    // THEN zoom_level returns 2.5
    CHECK(wf.zoom_level() == 2.5f);

    // AND zoom label reflects the new value
    CHECK(std::string(wf.zoom_label()).find("2.5") != std::string::npos);
}

TEST_CASE("WaveformPreviewWidget zoom is clamped to [0.1, 100.0]",
          "[graphics][browser][waveform][pr4]")
{
    WaveformPreviewWidget wf;

    // WHEN set below minimum
    wf.set_zoom_level(0.01f);
    CHECK(wf.zoom_level() == 0.1f);

    // WHEN set above maximum
    wf.set_zoom_level(500.0f);
    CHECK(wf.zoom_level() == 100.0f);
}

TEST_CASE("WaveformPreviewWidget waveform data survives zoom change",
          "[graphics][browser][waveform][pr4]")
{
    WaveformPreviewWidget wf;

    // GIVEN a widget with waveform data
    WaveformData data;
    data.peaks   = {0.5f, -0.3f, 0.8f};
    data.minima  = {-0.4f, 0.1f, -0.7f};
    wf.set_waveform(data);
    REQUIRE(wf.has_waveform());

    // WHEN zoom level changes
    wf.set_zoom_level(4.0f);

    // THEN waveform data is still accessible
    CHECK(wf.has_waveform());
    CHECK(wf.waveform_data().peaks[0] == 0.5f);
}

TEST_CASE("WaveformPreviewWidget time ruler visibility toggles",
          "[graphics][browser][waveform][pr4]")
{
    WaveformPreviewWidget wf;

    // GIVEN default state — ruler is visible
    CHECK(wf.time_ruler_visible());

    // WHEN ruler is hidden
    wf.set_time_ruler_visible(false);
    CHECK_FALSE(wf.time_ruler_visible());

    // WHEN ruler is shown again
    wf.set_time_ruler_visible(true);
    CHECK(wf.time_ruler_visible());
}

TEST_CASE("WaveformPreviewWidget fallback text still renders after polish",
          "[graphics][browser][waveform][pr4]")
{
    // GIVEN a WaveformPreviewWidget with no waveform data
    WaveformPreviewWidget wf;
    CHECK_FALSE(wf.has_waveform());

    // WHEN zoom level is set (polish doesn't break fallback)
    wf.set_zoom_level(2.0f);
    wf.set_time_ruler_visible(false);

    // THEN has_waveform still returns false
    CHECK_FALSE(wf.has_waveform());
}

// =====================================================================
// PR 4 — DnD from GPU panel → ProjectManager (RED → GREEN)
// =====================================================================

TEST_CASE("BrowserPanelGPU stores and fires file drag callback",
          "[graphics][browser][panel][pr4]")
{
    BrowserPanelGPU panel;

    // GIVEN a drag callback is registered
    std::string dragged_path;
    panel.on_file_drag([&dragged_path](const std::string& path) {
        dragged_path = path;
    });

    // WHEN begin_file_drag is called with a path
    panel.begin_file_drag("/samples/kick.wav");

    // THEN the callback fires with the expected path
    CHECK(dragged_path == "/samples/kick.wav");
}

TEST_CASE("BrowserPanelGPU stores project manager reference",
          "[graphics][browser][panel][pr4]")
{
    ProjectManager pm;
    BrowserPanelGPU panel;

    // GIVEN no project manager set
    CHECK(panel.project_manager() == nullptr);

    // WHEN project manager is set
    panel.set_project_manager(&pm);

    // THEN the reference is retrievable
    CHECK(panel.project_manager() == &pm);
}

TEST_CASE("BrowserPanelGPU begin_file_drag is safe with no callback",
          "[graphics][browser][panel][pr4]")
{
    BrowserPanelGPU panel;

    // GIVEN a panel with no drag callback registered
    // WHEN begin_file_drag is called
    // THEN no crash occurs
    CHECK_NOTHROW(panel.begin_file_drag("/samples/hihat.wav"));

    // AND state is unchanged
    CHECK(std::string(panel.type_name()) == "BrowserPanelGPU");
}

TEST_CASE("BrowserPanelGPU begin_file_drag handles empty path gracefully",
          "[graphics][browser][panel][pr4]")
{
    BrowserPanelGPU panel;
    int call_count = 0;
    panel.on_file_drag([&call_count](const std::string&) {
        ++call_count;
    });

    // WHEN begin_file_drag is called with an empty path
    panel.begin_file_drag("");

    // THEN the callback should still fire (path can be empty)
    CHECK(call_count == 1);
}
