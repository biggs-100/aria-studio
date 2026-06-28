// ARIA DAW — Application Entry Point

#include "kernel/application.h"
#include "kernel/crash_handler.h"

int main(int argc, char* argv[]) {
    aria::CrashHandler::install();

    auto& app = aria::Application::instance();
    if (!app.init(argc, argv)) {
        return 1;
    }

    int result = app.run();
    app.shutdown();
    return result;
}
