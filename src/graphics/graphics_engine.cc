#include "graphics/graphics_engine.h"
#include "kernel/service_locator.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

// ── Dawn headers (available after FetchContent) ───────────────────
#include <dawn/webgpu_cpp.h>
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>

namespace aria {

// =====================================================================
// Pimpl — Hide Dawn types from the public header
// =====================================================================

struct GraphicsEngine::Impl {
    BackendPreference preference;
    bool initialized = false;

    // Dawn types
    wgpu::Instance instance;
    wgpu::Device device;
    wgpu::Adapter adapter;

    // Dawn native instance (for adapter enumeration)
    std::unique_ptr<dawn::native::Instance> native_instance;

    // Adapter info
    AdapterInfo info;

    // Active swap chains (opaque handles owned by Dawn)
    struct SwapChain {
        wgpu::SwapChain chain;
        uint32_t width;
        uint32_t height;
        uint32_t format;
    };
    std::vector<std::unique_ptr<SwapChain>> swap_chains;

    explicit Impl(BackendPreference pref) : preference(pref) {}

    // ── Helpers ──────────────────────────────────────────────────

    static wgpu::BackendType dawn_backend_from_pref(BackendPreference p) {
        switch (p) {
        case BackendPreference::Null:
            return wgpu::BackendType::Null;
        case BackendPreference::DiscreteGpu:
        case BackendPreference::Default:
        default:
            return wgpu::BackendType::Undefined; // auto-select
        }
    }

    static AdapterType classify_adapter(wgpu::AdapterType t) {
        switch (t) {
        case wgpu::AdapterType::DiscreteGPU: return AdapterType::DiscreteGpu;
        case wgpu::AdapterType::IntegratedGPU: return AdapterType::IntegratedGpu;
        case wgpu::AdapterType::CPU: return AdapterType::Cpu;
        default: return AdapterType::Unknown;
        }
    }

    bool init_device() {
        // 1. Create the Dawn native instance and proc table
        dawnProcSetProcs(&dawn::native::GetProcs());

        native_instance = std::make_unique<dawn::native::Instance>();

        // 2. Discover adapters
        native_instance->DiscoverDefaultAdapters();
        std::vector<dawn::native::Adapter> adapters = native_instance->GetAdapters();

        if (adapters.empty()) {
            // No GPU adapter available — graceful failure
            return false;
        }

        // 3. Select the best adapter based on preference
        dawn::native::Adapter* selected = nullptr;
        wgpu::AdapterType selected_type = wgpu::AdapterType::DiscreteGPU;

        if (preference == BackendPreference::Null) {
            // Null backend: we still try to init but it won't create a
            // real device. This is used for headless / CI testing.
            // Dawn's null backend returns no usable adapters, so we
            // fall through to return false.
            return false;
        }

        // Order of preference: Discrete > Integrated > CPU
        for (auto& a : adapters) {
            wgpu::AdapterProperties props;
            a.GetProperties(&props);
            if (preference == BackendPreference::DiscreteGpu &&
                props.adapterType == wgpu::AdapterType::DiscreteGPU) {
                selected = &a;
                selected_type = props.adapterType;
                break;
            }
            // First pass: prefer discrete
            if (props.adapterType == wgpu::AdapterType::DiscreteGPU &&
                selected_type != wgpu::AdapterType::DiscreteGPU) {
                selected = &a;
                selected_type = props.adapterType;
            }
            // Second pass: fallback to integrated if nothing yet
            if (!selected && props.adapterType == wgpu::AdapterType::IntegratedGPU) {
                selected = &a;
                selected_type = props.adapterType;
            }
            // Third pass: CPU fallback
            if (!selected) {
                selected = &a;
                selected_type = props.adapterType;
            }
        }

        if (!selected) {
            return false;
        }

        // 4. Extract the wgpu::Adapter from the dawn::native adapter
        adapter = selected->Get();

        // 5. Request device
        wgpu::DeviceDescriptor deviceDesc{};
        // Required features: texture usage for swap chain
        std::vector<wgpu::FeatureName> required_features = {
            wgpu::FeatureName::SurfaceCapabilities,
        };
        deviceDesc.requiredFeaturesCount = static_cast<uint32_t>(required_features.size());
        deviceDesc.requiredFeatures = required_features.data();

        device = adapter.CreateDevice(&deviceDesc);
        if (!device) {
            return false;
        }

        // 6. Set up the device lost callback
        device.SetDeviceLostCallback(
            wgpu::CallbackMode::AllowSpontaneous,
            [](WGPUDevice const&, WGPUDeviceLostReason reason, char const* message) {
                if (reason != WGPUDeviceLostReason_Destroyed) {
                    // Log device loss (spdlog integration in future)
                    (void)message;
                }
            });

        // 7. Populate adapter info
        wgpu::AdapterProperties props;
        adapter.GetProperties(&props);
        info.type = classify_adapter(props.adapterType);
        info.name = props.name ? props.name : "";

        initialized = true;
        return true;
    }

    void shutdown_device() {
        initialized = false;
        swap_chains.clear();
        device = nullptr;
        adapter = nullptr;
        native_instance.reset();
        info = {};
    }

    SwapChain* find_swap_chain(WGPUSwapChainImpl* handle) {
        auto it = std::find_if(swap_chains.begin(), swap_chains.end(),
            [handle](const auto& sc) { return sc.get() == handle; });
        return (it != swap_chains.end()) ? it->get() : nullptr;
    }

    uint32_t next_swap_chain_id() const {
        return static_cast<uint32_t>(swap_chains.size());
    }
};

// =====================================================================
// Public API
// =====================================================================

GraphicsEngine::GraphicsEngine(BackendPreference preference)
    : impl_(std::make_unique<Impl>(preference)) {}

GraphicsEngine::~GraphicsEngine() { shutdown(); }

bool GraphicsEngine::init() {
    if (impl_->initialized) {
        return true; // already initialised
    }
    return impl_->init_device();
}

void GraphicsEngine::shutdown() {
    impl_->shutdown_device();
}

bool GraphicsEngine::is_initialized() const noexcept {
    return impl_->initialized;
}

AdapterInfo GraphicsEngine::adapter_info() const {
    return impl_->info;
}

// ── Swap chain ───────────────────────────────────────────────────

WGPUSwapChainImpl* GraphicsEngine::create_swapchain(const SwapChainConfig& cfg) {
    if (!impl_->initialized || !impl_->device) {
        return nullptr;
    }

    // Create a surface-less swap chain for now — in PR 2 / PR 3 this
    // will be backed by an OS window surface (via GLFW / SDL / native).
    // For the skeleton we use a basic swap chain descriptor.
    wgpu::SwapChainDescriptor scDesc{};
    scDesc.width = cfg.width;
    scDesc.height = cfg.height;
    scDesc.format = static_cast<wgpu::TextureFormat>(cfg.format);
    scDesc.usage = wgpu::TextureUsage::RenderAttachment;
    scDesc.presentMode = cfg.vsync
        ? wgpu::PresentMode::Fifo
        : wgpu::PresentMode::Immediate;

    auto wgpu_chain = impl_->device.CreateSwapChain(nullptr, &scDesc);
    if (!wgpu_chain) {
        return nullptr;
    }

    auto entry = std::make_unique<Impl::SwapChain>();
    entry->chain  = wgpu_chain;
    entry->width  = cfg.width;
    entry->height = cfg.height;
    entry->format = cfg.format;

    auto* raw = entry.get();
    impl_->swap_chains.push_back(std::move(entry));
    return reinterpret_cast<WGPUSwapChainImpl*>(raw);
}

void GraphicsEngine::resize_swapchain(WGPUSwapChainImpl* sc, uint32_t width, uint32_t height) {
    auto* entry = impl_->find_swap_chain(sc);
    if (!entry || !impl_->device) {
        return;
    }

    // Reconfigure the existing swap chain with the new dimensions.
    // Dawn does not provide a direct "resize" — we must recreate the
    // swap chain. The external handle remains valid; the internal
    // wgpu::SwapChain is replaced.
    wgpu::SwapChainDescriptor scDesc{};
    scDesc.width = width;
    scDesc.height = height;
    scDesc.format = static_cast<wgpu::TextureFormat>(entry->format);
    scDesc.usage = wgpu::TextureUsage::RenderAttachment;

    entry->chain = impl_->device.CreateSwapChain(nullptr, &scDesc);
    entry->width  = width;
    entry->height = height;
}

SwapChainInfo GraphicsEngine::query_swapchain_info(WGPUSwapChainImpl* sc) {
    SwapChainInfo info{};
    auto* entry = impl_->find_swap_chain(sc);
    if (entry) {
        info.width  = entry->width;
        info.height = entry->height;
        info.format = entry->format;
    }
    return info;
}

void GraphicsEngine::destroy_swapchain(WGPUSwapChainImpl* sc) {
    auto it = std::find_if(impl_->swap_chains.begin(), impl_->swap_chains.end(),
        [sc](const auto& entry) { return entry.get() == sc; });
    if (it != impl_->swap_chains.end()) {
        impl_->swap_chains.erase(it);
    }
    // If sc is null or not found, this is a no-op (safe / idempotent).
}

void GraphicsEngine::present(WGPUSwapChainImpl* sc) {
    if (!impl_ || !impl_->initialized || !sc) return;
    auto* entry = impl_->find_swap_chain(sc);
    if (entry && entry->chain) {
        entry->chain.Present();
    }
}

} // namespace aria
