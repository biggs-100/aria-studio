#ifndef ARIA_GRAPHICS_ENGINE_H
#define ARIA_GRAPHICS_ENGINE_H

#include <cstdint>
#include <memory>
#include <string>

struct WGPUSwapChainImpl;  // Opaque Dawn swap chain handle

namespace aria {

// ── Backend / adapter selection ───────────────────────────────────

/// Preference for GPU adapter selection during init().
enum class BackendPreference {
    Default,       ///< Let Dawn auto-select the best adapter.
    Null,          ///< Use Dawn's null (headless) backend — always fails init.
    DiscreteGpu,   ///< Prefer discrete GPU over integrated.
};

/// Adapter type reported by Dawn after a successful init().
enum class AdapterType {
    Unknown = 0,
    DiscreteGpu,
    IntegratedGpu,
    Cpu,
};

/// Properties of the selected GPU adapter.
struct AdapterInfo {
    AdapterType type = AdapterType::Unknown;
    std::string name;
};

// ── Swap chain configuration ──────────────────────────────────────

/// Descriptor passed to create_swapchain().
struct SwapChainConfig {
    uint32_t width  = 640;
    uint32_t height = 480;
    uint32_t format = 0;     ///< WGPUTextureFormat value.
    bool     vsync  = true;
};

/// Information queried from an active swap chain.
struct SwapChainInfo {
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t format = 0;
};

// ── GraphicsEngine ────────────────────────────────────────────────

/// Manages the Dawn WebGPU device, adapter selection, and swap chain
/// lifecycle. Registered with ServiceLocator for application-wide access.
///
/// Design decisions (from design.md):
///   - Dawn device init with discrete GPU fallback to integrated.
///   - Swap chains use WGPUTextureFormat_BGRA8Unorm.
///   - init() returns false when no adapter is available.
///   - Null backend is supported for headless / CI testing.
class GraphicsEngine {
public:
    explicit GraphicsEngine(BackendPreference preference = BackendPreference::Default);
    ~GraphicsEngine();

    GraphicsEngine(const GraphicsEngine&) = delete;
    GraphicsEngine& operator=(const GraphicsEngine&) = delete;
    GraphicsEngine(GraphicsEngine&&) = delete;
    GraphicsEngine& operator=(GraphicsEngine&&) = delete;

    /// Initialise the Dawn device and select an adapter.
    /// Returns true on success, false on failure (no GPU, null backend, etc.).
    bool init();

    /// Gracefully shut down the device and release all resources.
    void shutdown();

    /// True after a successful init() and before shutdown().
    bool is_initialized() const noexcept;

    /// Adapter information (valid only after init()).
    AdapterInfo adapter_info() const;

    // ── Swap chain ───────────────────────────────────────────────

    /// Create a swap chain from the given config.
    /// Returns nullptr if the engine is not initialised or device creation fails.
    WGPUSwapChainImpl* create_swapchain(const SwapChainConfig& cfg);

    /// Resize an existing swap chain (e.g. on window resize).
    void resize_swapchain(WGPUSwapChainImpl* sc, uint32_t width, uint32_t height);

    /// Query current swap chain dimensions and format.
    SwapChainInfo query_swapchain_info(WGPUSwapChainImpl* sc);

    /// Destroy a swap chain. Safe to call with nullptr or after destroy.
    void destroy_swapchain(WGPUSwapChainImpl* sc);

    /// Present the current frame to the swap chain.
    /// Safe to call with nullptr — no-op if sc is null or engine not initialized.
    void present(WGPUSwapChainImpl* sc);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aria

#endif // ARIA_GRAPHICS_ENGINE_H
