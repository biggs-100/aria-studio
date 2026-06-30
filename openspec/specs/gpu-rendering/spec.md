# GPU Rendering Specification

## Purpose

Manage Dawn WebGPU device lifecycle, swap chains, and Skia Ganesh GPU surfaces across one or more OS windows.

## Requirements

### Requirement: Device Initialization

The system SHALL initialize a Dawn WebGPU device with a discrete GPU adapter when `ARIA_FEATURE_GPU` is enabled. On failure, it SHALL fall back to an integrated GPU adapter. If no adapter is available, the system SHALL report an error and skip GPU initialization.

#### Scenario: Discrete GPU adapter selected

- GIVEN a system with a discrete GPU
- WHEN `GraphicsEngine::init()` is called
- THEN Dawn selects the discrete GPU adapter
- AND a `wgpu::Device` is created with graphics + present capabilities

#### Scenario: No GPU adapter available

- GIVEN a system with no GPU (headless or software-only)
- WHEN `GraphicsEngine::init()` is called
- THEN the system returns `false`
- AND GPU rendering is disabled fallback

### Requirement: Swap Chain and Surface

The system MUST create one swap chain per OS window. Each swap chain SHALL use `WGPUTextureFormat_BGRA8Unorm`. The swap chain SHALL be resized when the window is resized and recreated on window resize.

#### Scenario: Window resize triggers swap chain recreation

- GIVEN an active swap chain for window W
- WHEN W is resized from 800x600 to 1024x768
- THEN the swap chain is reconfigured to match the new size
- AND the next frame renders at the new resolution

#### Scenario: Multi-window swap chains

- GIVEN two windows W1 and W2
- WHEN both are visible and the render loop runs
- THEN each window has its own independent swap chain
- AND frames are presented independently per window

### Requirement: Skia Ganesh GPU Backend

The system MUST create a `SkiaCanvas` backed by a Skia Ganesh GPU `SkSurface`. Each surface SHALL be backed by a Dawn `WGPUTexture` from the swap chain. The `SkiaCanvas` SHALL expose `clear()`, `drawRect()`, `drawCircle()`, `drawTextBlob()`, and `flush()` operations.

#### Scenario: GPU-backed rectangle draw

- GIVEN an initialized SkiaCanvas with a Dawn backend
- WHEN `canvas->drawRect(rect, paint)` is called
- THEN a GPU draw command is issued via Skia Ganesh
- AND `canvas->flush()` queues the command to Dawn

#### Scenario: Surface recreation after swap chain change

- GIVEN a SkiaCanvas bound to swap chain S
- WHEN S is reconfigured (e.g., window resize)
- THEN the SkiaCanvas surface is recreated from the new swap chain texture
- AND no application-level draw code needs updating
