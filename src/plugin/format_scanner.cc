#include "format_scanner.h"

namespace aria::plugin {

bool FormatScanner::verify(const std::string& /*path*/) {
    // Base implementation: assume all plugins are valid.
    // Subclasses may override to perform actual format-specific verification.
    return true;
}

} // namespace aria::plugin