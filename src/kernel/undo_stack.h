#ifndef ARIA_UNDO_STACK_H
#define ARIA_UNDO_STACK_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace aria {

/// Undo/redo stack with snapshot history.
class UndoStack {
public:
    UndoStack() = default;
    ~UndoStack() = default;

    using UndoFunc = std::function<void()>;
    using RedoFunc = std::function<void()>;

    /// Push an undo/redo pair.
    void push(const std::string& label, UndoFunc undo, RedoFunc redo);

    /// Undo the last action.
    bool undo();

    /// Redo the last undone action.
    bool redo();

    /// Clear all history.
    void clear();

    uint32_t undo_count() const;
    uint32_t redo_count() const;
    std::string undo_label() const;
    std::string redo_label() const;

    void set_max_stack_size(uint32_t max_size);
    uint32_t max_stack_size() const;

private:
    struct Entry {
        std::string label;
        UndoFunc undo;
        RedoFunc redo;
    };

    std::vector<Entry> undo_stack_;
    std::vector<Entry> redo_stack_;
    uint32_t max_size_ = 256;
};

} // namespace aria

#endif // ARIA_UNDO_STACK_H
