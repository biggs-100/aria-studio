#ifndef ARIA_APPLICATION_H
#define ARIA_APPLICATION_H

#include <memory>
#include <string>

namespace aria {

class ServiceLocator;
class EventBus;
class CommandQueue;
class UndoStack;

/// Application — main application lifecycle manager.
class Application {
public:
    Application();
    ~Application();

    bool init(int argc, char* argv[]);
    int run();
    void shutdown();
    bool is_running() const;

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

    static Application* instance_;
};

} // namespace aria

#endif // ARIA_APPLICATION_H
