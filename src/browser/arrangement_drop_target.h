#pragma once

#include "kernel/event_types.h"
#include "project/project_manager.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace aria {
namespace browser {

/// Provisional arrangement drop target for browser drag-and-drop.
///
/// Displays track lanes that accept "BROWSER_FILE" ImGui drag-drop payloads
/// from the BrowserPanel. On drop, hit-tests mouse coordinates to determine
/// the target track and PPQN position, then calls
/// `ProjectManager::create_audio_clip()`.
///
/// This is a PROVISIONAL implementation. The full GPU arrangement view
/// (with waveform rendering, clip editing, etc.) comes in P10.
class ArrangementDropTarget {
public:
    explicit ArrangementDropTarget(ProjectManager* pm);
    ~ArrangementDropTarget() = default;

    // Non-copyable
    ArrangementDropTarget(const ArrangementDropTarget&) = delete;
    ArrangementDropTarget& operator=(const ArrangementDropTarget&) = delete;

    // ─── Drawing ──────────────────────────────────────────────

    /// Draw the arrangement drop-target lanes. Call once per ImGui frame.
    void draw();

    // ─── Zoom / scroll ───────────────────────────────────────

    void set_ppqn_per_pixel(double ppqn) { ppqn_per_pixel_ = ppqn; }
    double ppqn_per_pixel() const { return ppqn_per_pixel_; }

    void set_scroll_x(double scroll) { scroll_x_ = scroll; }
    double scroll_x() const { return scroll_x_; }

    // ─── Callback ─────────────────────────────────────────────

    /// Callback fired when a clip is created via drag-and-drop.
    using ClipCreatedCallback = std::function<void(ClipID id,
                                                    const std::string& filepath)>;

    /// Register a handler for clip-creation events.
    void on_clip_created(ClipCreatedCallback cb) { clip_created_cb_ = std::move(cb); }

private:
    // ─── State ───────────────────────────────────────────────

    ProjectManager* pm_ = nullptr;
    ClipCreatedCallback clip_created_cb_;

    // Zoom / scroll
    double ppqn_per_pixel_ = 4.0;  // Default: 4 PPQN per pixel (~960 PPQN per 240px bar)
    double scroll_x_       = 0.0;

    // Track layout cache (rebuilt each frame for hit testing)
    struct TrackRow {
        TrackID id;
        std::string name;
        float y_start = 0.0f;
        float y_end   = 0.0f;
    };
    std::vector<TrackRow> rows_;
};

} // namespace browser
} // namespace aria
