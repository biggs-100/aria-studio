#include "application.h"
#include "service_locator.h"
#include "event_bus.h"
#include "command_queue.h"
#include "undo_stack.h"

// GPU rendering integration (optional — guarded by ARIA_FEATURE_GPU)
#ifdef ARIA_FEATURE_GPU
#include "graphics/render_loop.h"
#include "graphics/graphics_engine.h"
#include "graphics/skia_canvas.h"
#endif

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

#ifdef ARIA_FEATURE_GPU
    // Register the GraphicsEngine service if available (created externally,
    // e.g. in main() or a platform-specific init path). The engine must be
    // registered via ServiceLocator before the render loop starts.
    if (services_->has_service<GraphicsEngine>()) {
        auto* engine = services_->get_service<GraphicsEngine>();
        if (engine && engine->init()) {
            // GraphicsEngine is ready — the render loop will be wired
            // once a SkiaCanvas and widget root are created externally.
        }
    }
#endif

    running_ = true;
    return true;
}

int Application::run() {
    while (running_) {
        // Main loop — process events, dispatch commands, render frame
        event_bus_->dispatch_pending();
        command_queue_->process_pending();

#ifdef ARIA_FEATURE_GPU
        // Tick the GPU render loop if configured
        if (render_loop_) {
            render_loop_->tick();
        }
#endif
    }
    return 0;
}

void Application::shutdown() {
#ifdef ARIA_FEATURE_GPU
    if (render_loop_) {
        render_loop_->stop();
    }
#endif
    running_ = false;
}

bool Application::is_running() const { return running_; }

ServiceLocator& Application::services() const { return *services_; }
EventBus& Application::events() const { return *event_bus_; }
CommandQueue& Application::commands() const { return *command_queue_; }
UndoStack& Application::undo() const { return *undo_stack_; }

Application& Application::instance() { return *instance_; }

} // namespace aria
