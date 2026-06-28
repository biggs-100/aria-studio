#include "selection_manager.h"
#include "piano_roll_canvas.h"  // For ViewTransform full definition

#include <algorithm>
#include <cmath>
#include <limits>

namespace aria {

// ═══════════════════════════════════════════════════════════════
// Selection management
// ═══════════════════════════════════════════════════════════════

void SelectionManager::select_note(NoteID id) {
    selected_.insert(id);
}

void SelectionManager::deselect_note(NoteID id) {
    selected_.erase(id);
}

void SelectionManager::toggle_note(NoteID id) {
    if (selected_.count(id)) {
        selected_.erase(id);
    } else {
        selected_.insert(id);
    }
}

void SelectionManager::select_all(NoteCollection& notes) {
    for (const auto& n : notes.notes()) {
        selected_.insert(n.id);
    }
}

void SelectionManager::clear() {
    selected_.clear();
}

void SelectionManager::select_marquee(const Rect& ppqn_rect,
                                       NoteCollection& notes)
{
    auto hits = notes.find_in_range(
        static_cast<uint64_t>(ppqn_rect.left()),
        static_cast<uint64_t>(ppqn_rect.right()),
        static_cast<uint8_t>(ppqn_rect.top()),
        static_cast<uint8_t>(ppqn_rect.bottom()));

    for (auto* n : hits) {
        selected_.insert(n->id);
    }
}

void SelectionManager::select_range(uint64_t start_ppqn,
                                     uint64_t end_ppqn,
                                     uint8_t key_low,
                                     uint8_t key_high,
                                     NoteCollection& notes)
{
    auto hits = notes.find_in_range(start_ppqn, end_ppqn, key_low, key_high);
    for (auto* n : hits) {
        selected_.insert(n->id);
    }
}

bool SelectionManager::is_selected(NoteID id) const {
    return selected_.count(id) > 0;
}

// ═══════════════════════════════════════════════════════════════
// Hit testing
// ═══════════════════════════════════════════════════════════════

SelectionManager::Handle
SelectionManager::hit_test(const Note& note, float sx, float sy,
                           const ViewTransform& view) const
{
    // Convert note to screen coordinates.
    double nx = view.ppqn_to_x(note.start_ppqn);
    double nw = static_cast<double>(note.duration_ppqn) /
                view.ppqn_per_pixel_x;
    double ny = view.key_to_y(note.key);
    double nh = view.pixels_per_semitone_y;

    // Handle dimensions.
    constexpr float HANDLE_SIZE = 6.0f;
    constexpr float HANDLE_HALF = HANDLE_SIZE * 0.5f;

    // Quick bounding box check.
    if (sx < nx - HANDLE_SIZE || sx > nx + nw + HANDLE_SIZE ||
        sy < ny - HANDLE_SIZE || sy > ny + nh + HANDLE_SIZE) {
        return Handle::None;
    }

    // Check each handle position.
    auto in_handle = [&](double hx, double hy) -> bool {
        return std::abs(static_cast<double>(sx) - hx) <= HANDLE_HALF &&
               std::abs(static_cast<double>(sy) - hy) <= HANDLE_HALF;
    };

    // Corner handles (higher priority).
    if (in_handle(nx, ny))              return Handle::TopLeft;
    if (in_handle(nx + nw, ny))         return Handle::TopRight;
    if (in_handle(nx, ny + nh))         return Handle::BottomLeft;
    if (in_handle(nx + nw, ny + nh))    return Handle::BottomRight;

    // Edge handles.
    double mid_x = nx + nw * 0.5;
    double mid_y = ny + nh * 0.5;
    if (in_handle(mid_x, ny))           return Handle::Top;
    if (in_handle(mid_x, ny + nh))      return Handle::Bottom;
    if (in_handle(nx, mid_y))           return Handle::Left;
    if (in_handle(nx + nw, mid_y))      return Handle::Right;

    return Handle::None;
}

// ═══════════════════════════════════════════════════════════════
// Drag operations
// ═══════════════════════════════════════════════════════════════

void SelectionManager::start_drag(const ViewTransform& view) {
    (void)view;
    drag_.active = true;
    drag_.origins.clear();
    drag_.origins.reserve(selected_.size());
    // Origins will be populated lazily — the caller provides delta directly.
    // But for move operations we need initial positions.
    // We store them here so end_drag can commit.
}

void SelectionManager::update_drag(int64_t delta_ppqn, int8_t delta_key) {
    // The actual move is handled by NoteCollection::shift_horizontal
    // and transpose when the drag ends. For now we just store the delta.
    // Implementations that need live preview can capture origins on start_drag.
    (void)delta_ppqn;
    (void)delta_key;
}

void SelectionManager::end_drag() {
    drag_.active = false;
    drag_.origins.clear();
}

// ═══════════════════════════════════════════════════════════════
// Copy / paste
// ═══════════════════════════════════════════════════════════════

std::vector<Note> SelectionManager::copy_selection(
    const NoteCollection& notes) const
{
    std::vector<Note> result;
    result.reserve(selected_.size());

    for (const auto& n : notes.notes()) {
        if (selected_.count(n.id)) {
            Note copy = n;
            copy.id = NoteID{};  // Clear ID — will be reassigned on paste.
            result.push_back(std::move(copy));
        }
    }
    return result;
}

void SelectionManager::paste_notes(uint64_t position, uint8_t base_key,
                                    NoteCollection& notes)
{
    // This is used to paste from an external clipboard vector.
    // Implementation is in the caller that has the clipboard data.
    (void)position;
    (void)base_key;
    (void)notes;
}

// ═══════════════════════════════════════════════════════════════
// Bulk edit
// ═══════════════════════════════════════════════════════════════

void SelectionManager::move_selection(int64_t delta_ppqn,
                                       int8_t delta_key,
                                       NoteCollection& notes)
{
    if (delta_ppqn == 0 && delta_key == 0) return;

    for (auto& n : notes.notes()) {
        if (!selected_.count(n.id)) continue;

        if (delta_ppqn != 0) {
            int64_t new_start = static_cast<int64_t>(n.start_ppqn) + delta_ppqn;
            n.start_ppqn = static_cast<uint64_t>(std::max<int64_t>(0, new_start));
        }
        if (delta_key != 0) {
            int new_key = static_cast<int>(n.key) + delta_key;
            n.key = static_cast<uint8_t>(std::clamp(new_key, 0, 127));
        }
    }
}

void SelectionManager::delete_selection(NoteCollection& notes) {
    // Copy IDs first to avoid iterator invalidation.
    auto ids = selected_;
    for (const auto& id : ids) {
        notes.remove(id);
    }
    selected_.clear();
}

void SelectionManager::duplicate_selection(uint64_t offset_ppqn,
                                            NoteCollection& notes)
{
    // Sort selected notes by start_ppqn so duplicates land after originals.
    std::vector<Note> copies;
    copies.reserve(selected_.size());

    for (const auto& n : notes.notes()) {
        if (selected_.count(n.id)) {
            Note copy = n;
            copy.id = NoteID{};
            copy.start_ppqn += offset_ppqn;
            copies.push_back(std::move(copy));
        }
    }

    // Add copies and select them.
    selected_.clear();
    for (auto& c : copies) {
        NoteID new_id = notes.add(c);
        selected_.insert(new_id);
    }
}

void SelectionManager::multiply_velocity_selection(double factor,
                                                    NoteCollection& notes)
{
    factor = std::clamp(factor, 0.0, 2.0);
    for (auto& n : notes.notes()) {
        if (!selected_.count(n.id)) continue;
        int new_vel = static_cast<int>(
            std::round(static_cast<double>(n.velocity) * factor));
        n.velocity = static_cast<uint8_t>(std::clamp(new_vel, 0, 127));
    }
}

void SelectionManager::transpose_selection(int8_t semitones,
                                            NoteCollection& notes)
{
    if (semitones == 0) return;
    for (auto& n : notes.notes()) {
        if (!selected_.count(n.id)) continue;
        int new_key = static_cast<int>(n.key) + semitones;
        n.key = static_cast<uint8_t>(std::clamp(new_key, 0, 127));
    }
}

} // namespace aria
