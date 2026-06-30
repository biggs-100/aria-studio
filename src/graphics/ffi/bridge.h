#ifndef ARIA_FFI_BRIDGE_H
#define ARIA_FFI_BRIDGE_H

#include "graphics/ffi/command_types.h"

#include <string>
#include <vector>

namespace aria {

class SkiaCanvas;

namespace ffi {

// ── Bridge ────────────────────────────────────────────────────────

/// Receives JSON command buffers from TypeScript and dispatches each
/// command to the active SkiaCanvas.
///
/// Handles HiDPI scaling: draw coordinates from TS (logical pixels)
/// are multiplied by the device pixel ratio before Skia execution.
///
/// Design decisions:
///   - Commands are dispatched in order as a single batch.
///   - A flush command at the end submits the frame to Dawn.
///   - Null/undefined buffers are safe no-ops.
class Bridge {
public:
    explicit Bridge(SkiaCanvas* canvas);

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    /// Execute a JSON command buffer.
    /// Parses the JSON, dispatches each command to SkiaCanvas,
    /// and calls flush if the buffer does not end with one.
    void execute(const std::string& json_buffer);

    // ── HiDPI ───────────────────────────────────────────────────

    /// Set the device pixel ratio for coordinate scaling.
    void set_device_pixel_ratio(float ratio);

    /// Current device pixel ratio.
    float device_pixel_ratio() const noexcept;

private:
    SkiaCanvas* canvas_;
    float dpr_ = 1.0f;

    /// Apply HiDPI scaling to a DrawCommand's coordinates.
    void apply_dpr(DrawCommand& cmd) const;

    /// Dispatch a single parsed command to SkiaCanvas.
    void dispatch(const DrawCommand& cmd);
};

} // namespace ffi
} // namespace aria

#endif // ARIA_FFI_BRIDGE_H
