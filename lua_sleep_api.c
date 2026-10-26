#include "lua_sleep_api.h"

static void lua_sleep_handler(lua_sched_co_ctx* coctx)
{

}

static int lua_ngx_sleep(lua_State *L)
{
    lua_sched_ctx* ctx;
    lua_sched_co_ctx* coctx = NULL;

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    int delay = (int)(luaL_checknumber(L, 1) * 1000);

    if (delay < 0) {
        return luaL_error(L, "invalid sleep duration \"%d\"", delay);
    }

    int nargs = lua_gettop(L);
    if (nargs == 0) {
        return luaL_error(L, "at least one thread should be specified");
    }

    coctx = ctx->cur_co_ctx;
    if (coctx == NULL) {
        return luaL_error(L, "no co ctx found");
    }

    if (coctx->sleep) {
        coctx->sleep->handler = lua_sleep_handler;
    }

    return lua_yield(L, 0);
}

void ngx_http_lua_inject_sleep_api(lua_State *L)
{
    lua_pushcfunction(L, lua_ngx_sleep);
    lua_setfield(L, -2, "sleep");
}
