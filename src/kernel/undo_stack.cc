#include "undo_stack.h"

namespace aria {

void UndoStack::push(const std::string& label, UndoFunc undo, RedoFunc redo) {
    if (undo_stack_.size() >= max_size_) {
        undo_stack_.erase(undo_stack_.begin());
    }
    undo_stack_.push_back({label, std::move(undo), std::move(redo)});
    redo_stack_.clear();
}

bool UndoStack::undo() {
    if (undo_stack_.empty()) return false;
    auto entry = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    if (entry.undo) entry.undo();
    redo_stack_.push_back(std::move(entry));
    return true;
}

bool UndoStack::redo() {
    if (redo_stack_.empty()) return false;
    auto entry = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (entry.redo) entry.redo();
    undo_stack_.push_back(std::move(entry));
    return true;
}

void UndoStack::clear() { undo_stack_.clear(); redo_stack_.clear(); }

uint32_t UndoStack::undo_count() const { return static_cast<uint32_t>(undo_stack_.size()); }
uint32_t UndoStack::redo_count() const { return static_cast<uint32_t>(redo_stack_.size()); }

std::string UndoStack::undo_label() const {
    return undo_stack_.empty() ? "" : undo_stack_.back().label;
}

std::string UndoStack::redo_label() const {
    return redo_stack_.empty() ? "" : redo_stack_.back().label;
}

void UndoStack::set_max_stack_size(uint32_t max_size) { max_size_ = max_size; }
uint32_t UndoStack::max_stack_size() const { return max_size_; }

} // namespace aria
