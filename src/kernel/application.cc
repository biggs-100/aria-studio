#include "application.h"
#include "service_locator.h"
#include "event_bus.h"
#include "command_queue.h"
#include "undo_stack.h"

namespace aria {

Application* Application::instance_ = nullptr;

Application::Application()
    : services_(std::make_unique<ServiceLocator>())
    , event_bus_(std::make_unique<EventBus>())
    , command_queue_(std::make_unique<CommandQueue>())
    , undo_stack_(std::make_unique<UndoStack>()) {
    instance_ = this;
}

Application::~Application() { shutdown(); }

bool Application::init(int /*argc*/, char* /*argv*/[]) {
    event_bus_->init();
    running_ = true;
    return true;
}

int Application::run() {
    while (running_) {
        // Main loop — process events, dispatch commands
        event_bus_->dispatch_pending();
        command_queue_->process_pending();
    }
    return 0;
}

void Application::shutdown() {
    running_ = false;
}

bool Application::is_running() const { return running_; }

ServiceLocator& Application::services() const { return *services_; }
EventBus& Application::events() const { return *event_bus_; }
CommandQueue& Application::commands() const { return *command_queue_; }
UndoStack& Application::undo() const { return *undo_stack_; }

Application& Application::instance() { return *instance_; }

} // namespace aria
