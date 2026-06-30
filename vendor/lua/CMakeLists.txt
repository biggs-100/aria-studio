# ── Vendored Lua 5.4.7 — STATIC library ─────────────────────
# Built from official Lua 5.4.7 source (minus lua.c/luac CLI).
# Exposes PUBLIC include path; linked as `lualib`.
#
# IMPORTANT: Lua MUST be compiled as C, not C++. sol2 (v3.3.0)
# wraps all Lua C-API includes in `extern "C" {}` (sol.hpp:2957),
# so it expects C-linkage symbols. Compiling Lua as C++ would
# produce C++-mangled symbols, causing LNK2001 errors when linking
# with sol2-consumed code.
#
# C++ consumers MUST include <lua.hpp> (which wraps headers in
# extern "C") or wrap #include <lua.h> in their own extern "C" block.

set(LUA_C_SOURCES
    lapi.c
    lauxlib.c
    lbaselib.c
    lcode.c
    lcorolib.c
    lctype.c
    ldblib.c
    ldebug.c
    ldo.c
    ldump.c
    lfunc.c
    lgc.c
    linit.c
    liolib.c
    llex.c
    lmathlib.c
    lmem.c
    loadlib.c
    lobject.c
    lopcodes.c
    loslib.c
    lparser.c
    lstate.c
    lstring.c
    lstrlib.c
    ltable.c
    ltablib.c
    ltm.c
    lundump.c
    lutf8lib.c
    lvm.c
    lzio.c
)

add_library(lualib STATIC ${LUA_C_SOURCES})

# Compiled as C (not C++) to produce C-linkage symbols matching
# sol2's extern "C" wrapping of Lua headers.
# On MSVC, /TC forces C compilation; on other compilers the .c
# extension is automatically recognized as C source.
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set_source_files_properties(${LUA_C_SOURCES} PROPERTIES LANGUAGE C)
endif()

target_include_directories(lualib PUBLIC include)

# Silence deprecation warnings from Lua's own code
target_compile_options(lualib PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/wd4996>)
target_compile_options(lualib PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>)
