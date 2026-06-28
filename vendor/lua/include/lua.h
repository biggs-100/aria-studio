// Minimal Lua header stub for build
extern "C" {
    typedef struct lua_State lua_State;
    lua_State* luaL_newstate(void) { return nullptr; }
    void lua_close(lua_State*) {}
}
