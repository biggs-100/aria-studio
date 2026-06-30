#include "browser/arrangement_drop_target.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// ImGui is vendored/pulled via FetchContent — guarded by ARIA_FEATURE_IMGUI.
// Provisional arrangement view for P9 browser DnD integration.
#if defined(ARIA_FEATURE_IMGUI)
#  include "imgui.h"
#endif

namespace aria {
namespace browser {

// ═══════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════

ArrangementDropTarget::ArrangementDropTarget(ProjectManager* pm)
    : pm_(pm) {
}

// ═══════════════════════════════════════════════════════════════
// Drawing
// ═══════════════════════════════════════════════════════════════

#if defined(ARIA_FEATURE_IMGUI)

void ArrangementDropTarget::draw() {
    if (!pm_) return;

    auto active_proj = pm_->active_project();
    if (active_proj.value == 0) {
        ImGui::Text("Arrangement: no active project");
        return;
    }

    auto& arr = pm_->get_arrangement(active_proj);
    auto track_ids = arr.track_order();

    if (track_ids.empty()) {
        ImGui::Text("Arrangement: no tracks");
        return;
    }

    // ── Draw tracks as drop-target lanes ──────────────────────

    rows_.clear();
    rows_.reserve(track_ids.size());

    const float lane_height = 48.0f;
    const float label_width = 120.0f;   // Track label column

    ImVec2 available = ImGui::GetContentRegionAvail();
    float view_width = std::max(available.x - label_width, 64.0f);

    ImGui::BeginChild("ArrangementDropScroll", ImVec2(0, 0), ImGuiChildFlags_Borders);

    // Get the child window's content area in screen coordinates
    ImVec2 child_min = ImGui::GetWindowContentRegionMin();
    ImVec2 child_pos = ImGui::GetWindowPos();
    float content_screen_x = child_pos.x + child_min.x;

    // Lane area screen X origin (after label column)
    float lane_screen_origin_x = content_screen_x + label_width + 4.0f;

    float y_cursor = ImGui::GetCursorScreenY();

    for (size_t i = 0; i < track_ids.size(); ++i) {
        TrackID tid = track_ids[i];
        auto* track = pm_->get_track(tid);
        if (!track) continue;

        std::string label = track->name();
        if (label.empty()) {
            switch (track->type()) {
                case TrackType::Audio:  label = "Audio " + std::to_string(i + 1); break;
                case TrackType::MIDI:   label = "MIDI " + std::to_string(i + 1);  break;
                case TrackType::Group:  label = "Group " + std::to_string(i + 1); break;
                case TrackType::VCA:    label = "VCA " + std::to_string(i + 1);   break;
                case TrackType::Return: label = "Return " + std::to_string(i + 1); break;
                case TrackType::Master: label = "Master"; break;
                default: label = "Track " + std::to_string(i + 1); break;
            }
        }

        float label_y = y_cursor + i * lane_height;

        // Track label column
        ImGui::SetCursorScreenPos(ImVec2(content_screen_x, label_y));
        ImGui::Text("%s", label.c_str());

        // Track lane — the drop target area
        ImGui::SetCursorScreenPos(ImVec2(lane_screen_origin_x, label_y));

        ImVec2 lane_size(view_width, lane_height);
        ImGui::InvisibleButton(("##arr_lane_" + std::to_string(tid.value)).c_str(),
                               lane_size);

        // Draw lane background
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 lane_bg = (i % 2 == 0)
                            ? IM_COL32(40, 40, 45, 255)
                            : IM_COL32(35, 35, 40, 255);
        dl->AddRectFilled(
            ImVec2(lane_screen_origin_x, label_y),
            ImVec2(lane_screen_origin_x + view_width, label_y + lane_height),
            lane_bg);

        // Highlight lane on hover
        if (ImGui::IsItemHovered()) {
            dl->AddRectFilled(
                ImVec2(lane_screen_origin_x, label_y),
                ImVec2(lane_screen_origin_x + view_width, label_y + lane_height),
                IM_COL32(60, 60, 80, 100));
        }

        // Register as drop target
        if (ImGui::BeginDragDropTarget()) {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("BROWSER_FILE");
            if (payload) {
                // Extract file path from payload (null-terminated string)
                const char* payload_str = static_cast<const char*>(payload->Data);
                std::string file_path(payload_str ? payload_str : "");

                // Hit-test mouse position → PPQN position
                float mouse_screen_x = ImGui::GetMousePos().x;
                float relative_x = mouse_screen_x - lane_screen_origin_x
                                 + static_cast<float>(scroll_x_);
                if (relative_x < 0) relative_x = 0;
                uint64_t position_ppqn = static_cast<uint64_t>(relative_x * ppqn_per_pixel_);

                // Check if this track type supports audio
                if (track->type() != TrackType::MIDI && track->type() != TrackType::Return) {
                    // Create the audio clip
                    ClipID clip_id = pm_->create_audio_clip(tid, position_ppqn, file_path);

                    if (clip_id.value != 0) {
                        // Emit clip-created event via callback
                        if (clip_created_cb_) {
                            clip_created_cb_(clip_id, file_path);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Draw grid lines (simple bar markers)
        ImU32 grid_color = IM_COL32(60, 60, 60, 80);
        double ppqn_per_bar = 960.0;  // 960 PPQN per bar at 4/4
        double start_bar_ppqn = std::floor((scroll_x_ * ppqn_per_pixel_) / ppqn_per_bar) * ppqn_per_bar;
        double end_bar_ppqn   = start_bar_ppqn + ((view_width / ppqn_per_pixel_) + ppqn_per_bar);
        for (double bar_ppqn = start_bar_ppqn; bar_ppqn < end_bar_ppqn; bar_ppqn += ppqn_per_bar) {
            float bx = lane_screen_origin_x
                     + static_cast<float>(bar_ppqn / ppqn_per_pixel_)
                     - static_cast<float>(scroll_x_);
            dl->AddLine(ImVec2(bx, label_y), ImVec2(bx, label_y + lane_height), grid_color);
        }

        // Cache row for reference
        TrackRow row;
        row.id      = tid;
        row.name    = label;
        row.y_start = label_y;
        row.y_end   = label_y + lane_height;
        rows_.push_back(std::move(row));
    }

    ImGui::SetCursorScreenY(y_cursor + track_ids.size() * lane_height + 4);
    ImGui::TextDisabled("Drag samples from browser onto tracks to create clips");

    ImGui::EndChild();
}

#else  // !ARIA_FEATURE_IMGUI

void ArrangementDropTarget::draw() {
    // ImGui not available — arrangement drop target is inactive.
    // Full arrangement view comes in P10.
}

#endif // ARIA_FEATURE_IMGUI

} // namespace browser
} // namespace aria
