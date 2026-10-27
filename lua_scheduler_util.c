#include "lua_scheduler_util.h"

/* Registry keys (as light userdata pointers) */
/* Registry keys */
char lua_sched_main_key;
char lua_sched_coroutines_key;
char lua_sched_co_ctx_key;
char lua_sched_ctx_registry_key;

void lua_sched_attach_co_ctx_to_L(lua_State* L,
                                  lua_State* co,
                                  lua_sched_co_ctx* coctx)
{
    lua_pushlightuserdata(L, lua_sched_lightudata_mask(co_ctx_key));
    lua_rawget(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, co);
    lua_pushlightuserdata(L, coctx);
    lua_rawset(L, -3);

    lua_pop(L, 1);
}

lua_sched_ctx* lua_registry_get(lua_State* L)
{
    lua_sched_ctx* ctx;

    lua_pushlightuserdata(L, lua_sched_lightudata_mask(ctx_registry_key));
    lua_rawget(L, LUA_REGISTRYINDEX);
    ctx = (lua_sched_ctx*)lua_touserdata(L, -1);
    lua_pop(L, 1);

    return ctx;
}

void lua_registry_set(lua_State* L, lua_sched_ctx* ctx)
{
    lua_pushlightuserdata(L, lua_sched_lightudata_mask(ctx_registry_key));
    lua_pushlightuserdata(L, ctx);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

lua_sched_ctx* lua_main_get(lua_State* L)
{
    lua_sched_ctx* ctx;

    lua_pushlightuserdata(L, lua_sched_lightudata_mask(main_key));
    lua_rawget(L, LUA_REGISTRYINDEX);
    ctx = (lua_sched_ctx*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return ctx;
}

void lua_main_set(lua_State* L, lua_sched_ctx* ctx)
{
    lua_pushlightuserdata(L, lua_sched_lightudata_mask(main_key));
    lua_pushlightuserdata(L, ctx);
    lua_rawset(L, LUA_REGISTRYINDEX);
}