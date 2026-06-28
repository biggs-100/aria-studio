#include "automation_engine.h"

namespace aria {

AutomationEngine::AutomationEngine() = default;
AutomationEngine::~AutomationEngine() { shutdown(); }

bool AutomationEngine::init() { initialized_ = true; return true; }
void AutomationEngine::shutdown() { initialized_ = false; }

} // namespace aria
