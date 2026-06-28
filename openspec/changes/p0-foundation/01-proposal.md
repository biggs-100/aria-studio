# SDD Change: P0 - Foundation

## Proposal

### Goal
Establish the complete development infrastructure for ARIA DAW: repository, build system, project structure, developer environment, core kernel, and testing framework.

### Scope
1. **Repository** — Git init, branching strategy, .gitignore, commit conventions
2. **Build system** — CMake (C++), Cargo (Rust), pnpm (TypeScript), cross-language integration
3. **Project structure** — Directory layout, module stubs, C++/Rust FFI skeleton
4. **Developer environment** — Setup script, Docker, VSCode/Cursor config, debug build
5. **Core kernel** — Application lifecycle, event bus, service locator, crash handler
6. **Testing infrastructure** — Catch2, audio test harness, test plugin suite, CI integration

### Out of Scope
- Any audio processing (P1)
- Any MIDI processing (P2)
- Any UI rendering (P10)
- Any plugin loading (P6)

### References
- `03_System_Architecture.md` — Architecture
- `20_Testing.md` — Testing strategy
- `21_Build_System.md` — Build system spec
