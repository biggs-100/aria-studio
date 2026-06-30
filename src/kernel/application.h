#ifndef ARIA_APPLICATION_H
#define ARIA_APPLICATION_H

#include <memory>
#include <string>

namespace aria {

class ServiceLocator;
class EventBus;
class CommandQueue;
class UndoStack;
class RenderLoop;
class GraphicsEngine;
class SkiaCanvas;
class ScriptManager;

/// Application — main application lifecycle manager.
class Application {
public:
    Application();
    ~Application();

    bool init(int argc, char* argv[]);
    int run();
    void shutdown();
    bool is_running() const;

    /// Register a GPU RenderLoop for the main rendering loop.
    /// Ownership remains with the caller; the Application stores a
    /// non-owning pointer. Pass nullptr to disable GPU rendering.
    void set_render_loop(RenderLoop* loop) { render_loop_ = loop; }

    /// Register a ScriptManager for the main loop tick.
    /// Ownership remains with the caller; pass nullptr if scripting is disabled.
    void set_script_manager(ScriptManager* mgr) { script_manager_ = mgr; }

    ServiceLocator& services() const;
    EventBus& events() const;
    CommandQueue& commands() const;
    UndoStack& undo() const;

    static Application& instance();

private:
    bool running_ = false;
    std::unique_ptr<ServiceLocator> services_;
    std::unique_ptr<EventBus> event_bus_;
    std::unique_ptr<CommandQueue> command_queue_;
    std::unique_ptr<UndoStack> undo_stack_;

    // GPU rendering (optional, non-owning pointers managed by init())
    RenderLoop* render_loop_ = nullptr;

    // Scripting (optional, non-owning — registered via ServiceLocator)
    ScriptManager* script_manager_ = nullptr;

    static Application* instance_;
};

} // namespace aria

#endif // ARIA_APPLICATION_H
