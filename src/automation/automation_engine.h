#ifndef ARIA_AUTOMATION_ENGINE_H
#define ARIA_AUTOMATION_ENGINE_H

#include <cstdint>

namespace aria {

/// Automation Engine — curves, modulation sources, and parameter automation.
class AutomationEngine {
public:
    AutomationEngine();
    ~AutomationEngine();

    bool init();
    void shutdown();

private:
    bool initialized_ = false;
};

} // namespace aria

#endif // ARIA_AUTOMATION_ENGINE_H
