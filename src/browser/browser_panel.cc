#include "browser/browser_panel.h"

#include "kernel/application.h"
#include "kernel/event_bus.h"
#include "project/project_manager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

// ImGui is vendored/pulled via FetchContent — still available as a fallback
// when GPU rendering is not enabled.
#if defined(ARIA_FEATURE_IMGUI)
#  include "imgui.h"
#endif

namespace aria {
namespace browser {

// ═══════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════

BrowserPanel::BrowserPanel(BrowserEngine* engine,
                             ProjectManager* pm)
    : engine_(engine) {
    if (engine_) {
        auto& bus = Application::instance().events();
    }

    // Create GPU panel when GPU rendering is available
#ifdef ARIA_FEATURE_GPU
    gpu_panel_ = std::make_unique<BrowserPanelGPU>(engine_);
    if (gpu_panel_ && pm) {
        gpu_panel_->set_project_manager(pm);

        // Wire GPU panel drag events → clip creation + EventBus
        gpu_panel_->on_file_drag(
            [this, pm](const std::string& filepath) {
                if (filepath.empty()) return;

                // Find the first audio track to create the clip on
                ProjectID active = pm->active_project();
                if (active.value == 0) return;

                size_t track_count = pm->track_count(active);
                TrackID first_audio{0};
                for (size_t i = 0; i < track_count; ++i) {
                    // Get track by index — use arrangement track_order
                    auto& arr = pm->get_arrangement(active);
                    auto ids = arr.track_order();
                    if (i < ids.size()) {
                        auto* tr = pm->get_track(ids[i]);
                        if (tr && (tr->type() == TrackType::Audio ||
                                   tr->type() == TrackType::Group)) {
                            first_audio = ids[i];
                            break;
                        }
                    }
                }

                if (first_audio.value == 0) return;

                // Create the audio clip at PPQN position 0
                ClipID clip_id = pm->create_audio_clip(first_audio, 0, filepath);
                if (clip_id.value == 0) return;

                // Publish clip-created event to EventBus
                auto& bus = Application::instance().events();
                BrowserEvent ev;
                ev.type      = BrowserEventType::kClipCreated;
                ev.timestamp = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count());
                ev.int_value = static_cast<int64_t>(clip_id.value);
                std::strncpy(ev.str_data, filepath.c_str(),
                             sizeof(ev.str_data) - 1);
                ev.str_data[sizeof(ev.str_data) - 1] = '\0';
                bus.publish(ev);

                // Also fire the drag callback with the file path
                if (drag_cb_) {
                    drag_cb_(filepath);
                }
            });
    }
#endif

    // Create provisional arrangement drop target.
    if (pm) {
        arrangement_drop_ = std::make_unique<ArrangementDropTarget>(pm);
        arrangement_drop_->on_clip_created(
            [this](ClipID id, const std::string& filepath) {
                // Forward clip-created event to EventBus
                auto& bus = Application::instance().events();
                BrowserEvent ev;
                ev.type      = BrowserEventType::kClipCreated;
                ev.timestamp = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    ).count());
                ev.int_value = static_cast<int64_t>(id.value);
                std::strncpy(ev.str_data, filepath.c_str(),
                             sizeof(ev.str_data) - 1);
                ev.str_data[sizeof(ev.str_data) - 1] = '\0';
                bus.publish(ev);

                // Also fire the drag callback with the file path
                if (drag_cb_) {
                    drag_cb_(filepath);
                }
            });
    }
}

void BrowserPanel::set_view_mode(ViewMode mode) {
    view_mode_ = mode;
#ifdef ARIA_FEATURE_GPU
    if (gpu_panel_) {
        switch (mode) {
            case ViewMode::FolderTree:
                gpu_panel_->set_view_mode(BrowserPanelGPU::ViewMode::FolderTree);
                break;
            case ViewMode::CategoryTree:
                gpu_panel_->set_view_mode(BrowserPanelGPU::ViewMode::CategoryTree);
                break;
            case ViewMode::SearchResults:
                gpu_panel_->set_view_mode(BrowserPanelGPU::ViewMode::SearchResults);
                break;
        }
    }
#endif
}

// ═══════════════════════════════════════════════════════════════
// Main draw call
// ═══════════════════════════════════════════════════════════════

void BrowserPanel::draw() {
    if (!engine_ || !engine_->is_initialized()) return;

#ifdef ARIA_FEATURE_GPU
    // GPU rendering path: delegate to BrowserPanelGPU
    // The panel is painted via the render loop; this draw() call
    // updates state and marks the GPU panel as dirty.
    if (gpu_panel_) {
        gpu_panel_->set_search_text(search_buffer_);
        return;
    }
#endif

    // Fallback: ImGui path (when defined) or stubs
#if defined(ARIA_FEATURE_IMGUI)
    draw_toolbar();

    switch (view_mode_) {
        case ViewMode::FolderTree:
            draw_folder_tree();
            break;
        case ViewMode::CategoryTree:
            draw_category_tree();
            break;
        case ViewMode::SearchResults:
            draw_search_results();
            break;
    }

    draw_preview_panel();

    if (arrangement_drop_) {
        arrangement_drop_->draw();
    }
#endif
}

// ─── ImGui draw helpers (fallback when GPU is not enabled) ─────────

#if defined(ARIA_FEATURE_IMGUI)

void BrowserPanel::draw_toolbar() {
    if (ImGui::Button("Folders")) {
        view_mode_ = ViewMode::FolderTree;
    }
    ImGui::SameLine();
    if (ImGui::Button("Categories")) {
        view_mode_ = ViewMode::CategoryTree;
        category_tree_dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Search")) {
        view_mode_ = ViewMode::SearchResults;
    }

    ImGui::SameLine();

    ImGui::InputText("##search", search_buffer_, sizeof(search_buffer_));

    ImGui::SameLine();
    if (ImGui::Button("Go") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (std::strlen(search_buffer_) > 0) {
            view_mode_ = ViewMode::SearchResults;
            execute_search();
        }
    }

    ImGui::SameLine();
    if (engine_->is_scanning()) {
        ImGui::Text("Scanning... %.0f%%",
                    engine_->scan_progress() * 100.0);
    } else {
        if (ImGui::Button("Scan")) {
            engine_->start_scan();
        }
    }
}

void BrowserPanel::draw_search_bar() {
    ImGui::InputText("##search", search_buffer_, sizeof(search_buffer_));
    ImGui::SameLine();
    if (ImGui::Button("Search")) {
        execute_search();
    }
}

void BrowserPanel::execute_search() {
    if (!engine_ || std::strlen(search_buffer_) == 0) return;

    SearchParams params;
    params.text = search_buffer_;
    params.limit = 200;

    current_results_ = engine_->search()->search_sync(params);
    results_dirty_ = false;
}

void BrowserPanel::draw_folder_tree() {
    if (ImGui::BeginChild("FolderTree", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
        for (const auto& folder : engine_->watched_folders()) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                                     | ImGuiTreeNodeFlags_SpanFullWidth;
            bool open = ImGui::TreeNodeEx(folder.c_str(), flags);

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            }

            if (open) {
                ImGui::Text("(query DB for subdirectories)");
                ImGui::TreePop();
            }
        }
    }
    ImGui::EndChild();
}

void BrowserPanel::build_category_tree(std::vector<CategoryNode>& nodes) {
    nodes.clear();
    nodes.push_back({"Kicks", 0, {}});
    nodes.push_back({"Snares", 0, {}});
    nodes.push_back({"HiHats", 0, {}});
    nodes.push_back({"Percussion", 0, {}});
    nodes.push_back({"Bass", 0, {}});
    nodes.push_back({"Pads", 0, {}});
    nodes.push_back({"Leads", 0, {}});
    nodes.push_back({"FX", 0, {}});
    nodes.push_back({"Loops", 0, {}});
    nodes.push_back({"Voices", 0, {}});

    if (engine_ && engine_->search()) {
        for (auto& node : nodes) {
            SearchParams params;
            params.category = node.name;
            params.limit = 1;
            auto result = engine_->search()->search_sync(params);
            node.count = result.total_count;
        }
    }
}

void BrowserPanel::draw_category_tree() {
    if (category_tree_dirty_) {
        build_category_tree(category_nodes_);
        category_tree_dirty_ = false;
    }

    if (ImGui::BeginChild("CategoryTree", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
        for (const auto& node : category_nodes_) {
            std::string label = node.name + " (" + std::to_string(node.count) + ")";

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
                                     | ImGuiTreeNodeFlags_NoTreePushOnOpen
                                     | ImGuiTreeNodeFlags_SpanFullWidth;
            ImGui::TreeNodeEx(label.c_str(), flags);

            if (ImGui::IsItemClicked()) {
                SearchParams params;
                params.category = node.name;
                params.limit = 200;
                current_results_ = engine_->search()->search_sync(params);
                results_dirty_ = false;
            }

            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("BROWSER_CATEGORY", node.name.c_str(),
                                          node.name.size() + 1);
                ImGui::Text("Drag %s", node.name.c_str());
                ImGui::EndDragDropSource();
            }
        }
    }
    ImGui::EndChild();
}

void BrowserPanel::draw_search_results() {
    if (ImGui::BeginChild("SearchResults", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
        if (results_dirty_) {
            execute_search();
        }

        if (std::strlen(search_buffer_) > 0 &&
            current_results_.samples.empty()) {
            execute_search();
        }

        ImGui::Text("%d results", current_results_.total_count);
        ImGui::Separator();

        for (size_t i = 0; i < current_results_.samples.size(); ++i) {
            const auto& sample = current_results_.samples[i];

            std::string display_name = sample.name;
            if (display_name.empty()) {
                std::string path = sample.file_path;
                auto pos = path.find_last_of("/\\");
                display_name = (pos != std::string::npos) ? path.substr(pos + 1) : path;
            }

            bool is_selected = (selected_file_ == sample.file_path);
            if (ImGui::Selectable(display_name.c_str(), &is_selected,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                selected_file_ = sample.file_path;
                selected_files_ = {sample.file_path};

                if (ImGui::IsMouseDoubleClicked(0)) {
                    if (engine_->preview()) {
                        engine_->preview()->play(sample.file_path);
                    }
                }

                if (sel_cb_) {
                    sel_cb_(sample.file_path);
                }
            }

            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("BROWSER_FILE", sample.file_path,
                                          std::strlen(sample.file_path) + 1);
                ImGui::Text("Drag %s", display_name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Play Preview")) {
                    if (engine_->preview()) {
                        engine_->preview()->play(sample.file_path);
                    }
                }
                if (ImGui::MenuItem("Stop Preview")) {
                    if (engine_->preview()) {
                        engine_->preview()->stop();
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Reveal in Explorer")) {
                }
                ImGui::EndPopup();
            }
        }
    }
    ImGui::EndChild();
}

void BrowserPanel::draw_preview_panel() {
    if (selected_file_.empty()) return;

    if (ImGui::BeginChild("PreviewPanel", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
        std::string filename = selected_file_;
        auto pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) filename = filename.substr(pos + 1);
        ImGui::Text("%s", filename.c_str());

        ImGui::Separator();

        ImVec2 waveform_size(0, 80);
        if (ImGui::BeginChild("Waveform", waveform_size, ImGuiChildFlags_Borders)) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImVec2 p_min = ImGui::GetCursorScreenPos();
            ImVec2 p_max(p_min.x + ImGui::GetContentRegionAvail().x,
                         p_min.y + 80);

            draw->AddRectFilled(p_min, p_max, IM_COL32(30, 30, 30, 255));

            auto wf = engine_->waveforms()->get_waveform(selected_file_, 256);
            if (wf && !wf->peaks.empty()) {
                float width = p_max.x - p_min.x - 4;
                float height = p_max.y - p_min.y - 4;
                float center_y = p_min.y + 2 + height * 0.5f;
                float step = width / static_cast<float>(wf->peaks.size());

                for (size_t i = 0; i < wf->peaks.size(); ++i) {
                    float x = p_min.x + 2 + static_cast<float>(i) * step;
                    float peak_h = (wf->peaks[i] * 0.5f + 0.5f) * height * 0.5f;
                    float min_h  = (wf->minima[i] * 0.5f + 0.5f) * height * 0.5f;
                    draw->AddLine(ImVec2(x, center_y - peak_h),
                                  ImVec2(x, center_y + min_h),
                                  IM_COL32(100, 200, 255, 255));
                }
            } else {
                draw->AddLine(ImVec2(p_min.x + 2, p_min.y + 40),
                              ImVec2(p_max.x - 2, p_min.y + 40),
                              IM_COL32(80, 80, 80, 255));
                draw->AddText(ImVec2(p_min.x + 10, p_min.y + 30),
                              IM_COL32(128, 128, 128, 255),
                              "No waveform data");
            }
        }
        ImGui::EndChild();

        auto* preview = engine_->preview();
        if (preview) {
            if (preview->is_playing()) {
                if (ImGui::Button("Stop")) {
                    preview->stop();
                }
                ImGui::SameLine();
                if (ImGui::Button("Pause")) {
                    preview->pause();
                }
            } else if (preview->is_paused()) {
                if (ImGui::Button("Resume")) {
                    preview->resume();
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    preview->stop();
                }
            } else {
                if (ImGui::Button("Play")) {
                    preview->play(selected_file_);
                }
            }

            ImGui::SameLine();
            float vol = static_cast<float>(preview->volume());
            if (ImGui::SliderFloat("##vol", &vol, -60.0f, 12.0f, "%.1f dB")) {
                preview->set_volume(vol);
            }
        }

        ImGui::Separator();

        ImGui::Text("File: %s", selected_file_.c_str());

        for (const auto& sample : current_results_.samples) {
            if (sample.file_path == selected_file_) {
                if (sample.bpm > 0) {
                    ImGui::Text("BPM: %.0f", sample.bpm);
                }
                if (std::strlen(sample.musical_key) > 0) {
                    ImGui::Text("Key: %s", sample.musical_key);
                }
                if (sample.duration_ms > 0) {
                    ImGui::Text("Duration: %.1f s",
                                sample.duration_ms / 1000.0);
                }
                if (sample.loudness_lufs > -100.0) {
                    ImGui::Text("Loudness: %.1f LUFS", sample.loudness_lufs);
                }
                if (sample.rating > 0) {
                    ImGui::Text("Rating: %d/5", sample.rating);
                }
                break;
            }
        }
    }
    ImGui::EndChild();
}

#else  // !ARIA_FEATURE_IMGUI

// Stub implementations when ImGui is not available
void BrowserPanel::draw_toolbar() {}
void BrowserPanel::draw_search_bar() {}
void BrowserPanel::execute_search() {}
void BrowserPanel::draw_folder_tree() {}
void BrowserPanel::draw_category_tree() {}
void BrowserPanel::build_category_tree(std::vector<CategoryNode>&) {}
void BrowserPanel::draw_search_results() {}
void BrowserPanel::draw_preview_panel() {}

#endif // ARIA_FEATURE_IMGUI

} // namespace browser
} // namespace aria
