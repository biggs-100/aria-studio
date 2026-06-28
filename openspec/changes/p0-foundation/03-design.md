# P0 - Foundation: Design

Design reference:
- `03_System_Architecture.md` §9 (Build System Architecture), §10 (Multi-Language Integration)
- `21_Build_System.md` §2 (Build Architecture), §3 (CMake), §4 (Cargo), §5 (pnpm)
- `20_Testing.md` §10 (CI/CD Integration)

Key architectural decisions:
1. CMake orchestrates the entire build, calling Cargo and pnpm as external steps
2. Rust compiles to a C ABI static library linked by C++
3. TypeScript bundles to a single JS file embedded in the C++ binary
4. Kernel uses a service locator pattern (not DI container)
5. Event bus is typed and lock-free for hot paths
