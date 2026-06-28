# ── Compiler Options ──────────────────────────────────────────

if(MSVC)
    add_compile_options(
        /W4
        # WX disabled: pre-existing pianoroll warnings need fixing upstream
        # /WX                  # Warnings as errors
        /utf-8
        /arch:AVX2
        /fp:fast
        /GL                    # Whole program optimization
        /EHsc                  # C++ exceptions (not SEH)
        /permissive-
        /Zc:preprocessor
        /Zc:__cplusplus
    )

    if(ARIA_ENABLE_ASAN)
        add_compile_options(/fsanitize=address)
    endif()

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Werror
        -Wno-c99-extensions
        -march=native
        -ffast-math
        -fvisibility=hidden
    )

    if(ARIA_ENABLE_ASAN)
        add_compile_options(-fsanitize=address)
    endif()
    if(ARIA_ENABLE_UBSAN)
        add_compile_options(-fsanitize=undefined)
    endif()

    if(APPLE)
        add_compile_options(-ObjC++)
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Werror
        -march=native
        -ffast-math
        -fvisibility=hidden
        -fno-omit-frame-pointer
    )

    if(ARIA_ENABLE_ASAN)
        add_compile_options(-fsanitize=address)
    endif()
    if(ARIA_ENABLE_UBSAN)
        add_compile_options(-fsanitize=undefined)
    endif()
endif()

# ── LTO ──────────────────────────────────────────────────────
if(ARIA_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_supported)
    if(lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
endif()

# ── Debug / Release flags ─────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(ARIA_DEBUG ARIA_LOGGING)
else()
    add_compile_definitions(NDEBUG)
endif()
