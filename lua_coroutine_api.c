#include "lua_coroutine_api.h"

#include <string.h>
#include <stdio.h>

#include "lua_scheduler.h"
/* Forward declarations */
static int lua_sched_coroutine_create_wrapper(lua_State* L);
static int lua_sched_coroutine_wrap_wrapper(lua_State* L);
static int lua_sched_coroutine_resume_wrapper(lua_State* L);
static int lua_sched_coroutine_yield_wrapper(lua_State* L);
static int lua_sched_coroutine_status_wrapper(lua_State* L);
static int lua_sched_coroutine_wrap_runner(lua_State* L);

/* coroutine.create wrapper */
static int lua_sched_coroutine_create_wrapper(lua_State* L)
{
    lua_sched_ctx* ctx;

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    return lua_sched_coroutine_create(L, ctx, NULL, NULL);
}

/* coroutine.wrap runner */
static int lua_sched_coroutine_wrap_runner(lua_State* L)
{
    /* Retrieve closure and insert it at the bottom of stack */
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);

    return lua_sched_coroutine_resume_wrapper(L);
}

/* coroutine.wrap wrapper */
static int lua_sched_coroutine_wrap_wrapper(lua_State* L)
{
    lua_sched_ctx* ctx;
    lua_sched_co_ctx* coctx = NULL;

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    lua_sched_coroutine_create(L, ctx, &coctx, NULL);

    if (coctx) {
        coctx->is_wrap = 1;
    }

    lua_pushcclosure(L, lua_sched_coroutine_wrap_runner, 1);

    return 1;
}

/* coroutine.resume wrapper */
static int lua_sched_coroutine_resume_wrapper(lua_State* L)
{
    lua_State* co;
    lua_sched_ctx* ctx;
    lua_sched_co_ctx* coctx;
    lua_sched_co_ctx* p_coctx;

    co = lua_tothread(L, 1);
    if (!co) {
        return luaL_argerror(L, 1, "coroutine expected");
    }

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    p_coctx = ctx->cur_co_ctx;
    if (p_coctx == NULL) {
        return luaL_error(L, "no parent co ctx found");
    }

    coctx = lua_sched_get_co_ctx(co, ctx);
    if (coctx == NULL) {
        return luaL_error(L, "no co ctx found");
    }

    if (coctx->co_status != LUA_SCHED_CO_SUSPENDED) {
        const char* status_names[] = {
            "running", "suspended", "normal", "dead", "zombie"
        };
        lua_pushboolean(L, 0);
        lua_pushfstring(L, "cannot resume %s coroutine",
                        status_names[coctx->co_status]);
        return 2;
    }

    p_coctx->co_status = LUA_SCHED_CO_NORMAL;
    coctx->parent_co_ctx = p_coctx;
    coctx->co_status = LUA_SCHED_CO_RUNNING;

    ctx->co_op = LUA_SCHED_USER_CORO_RESUME;
    ctx->cur_co_ctx = coctx;

    /* Yield and pass args to scheduler, resume target coroutine from there */
    /* This matches OpenResty's implementation exactly */
    return lua_yield(L, lua_gettop(L) - 1);
}

/* coroutine.yield wrapper */
static int lua_sched_coroutine_yield_wrapper(lua_State* L)
{
    lua_sched_ctx* ctx;
    lua_sched_co_ctx* coctx;

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    coctx = ctx->cur_co_ctx;
    if (coctx == NULL) {
        return luaL_error(L, "no coroutine context found");
    }

    coctx->co_status = LUA_SCHED_CO_SUSPENDED;
    ctx->co_op = LUA_SCHED_USER_CORO_YIELD;

    if (!coctx->is_uthread && coctx->parent_co_ctx) {
        coctx->parent_co_ctx->co_status = LUA_SCHED_CO_RUNNING;
    }

    /* Yield and pass return values to scheduler */
    return lua_yield(L, lua_gettop(L));
}

/* coroutine.status wrapper */
static int lua_sched_coroutine_status_wrapper(lua_State* L)
{
    lua_State* co;
    lua_sched_ctx* ctx;
    lua_sched_co_ctx* coctx;

    co = lua_tothread(L, 1);
    if (!co) {
        return luaL_argerror(L, 1, "coroutine expected");
    }

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    coctx = lua_sched_get_co_ctx(co, ctx);
    if (coctx == NULL) {
        lua_pushliteral(L, "dead");
        return 1;
    }

    const char* status_names[] = {
        "running", "suspended", "normal", "dead", "zombie"
    };

    lua_pushstring(L, status_names[coctx->co_status]);
    return 1;
}

/* Inject coroutine API into Lua state */
void lua_sched_inject_coroutine_api(lua_State* L)
{
    /* Create new coroutine table */
    lua_createtable(L, 0, 16);

    /* Get old coroutine table */
    lua_getglobal(L, "coroutine");

    /* Copy standard functions with underscore prefix */
    lua_getfield(L, -1, "running");
    lua_setfield(L, -3, "running");

    lua_getfield(L, -1, "create");
    lua_setfield(L, -3, "_create");

    lua_getfield(L, -1, "wrap");
    lua_setfield(L, -3, "_wrap");

    lua_getfield(L, -1, "resume");
    lua_setfield(L, -3, "_resume");

    lua_getfield(L, -1, "yield");
    lua_setfield(L, -3, "_yield");

    lua_getfield(L, -1, "status");
    lua_setfield(L, -3, "_status");

    /* Pop the old coroutine table */
    lua_pop(L, 1);

    /* Add our custom functions */
    lua_pushcfunction(L, lua_sched_coroutine_create_wrapper);
    lua_setfield(L, -2, "__create");

    lua_pushcfunction(L, lua_sched_coroutine_wrap_wrapper);
    lua_setfield(L, -2, "__wrap");

    lua_pushcfunction(L, lua_sched_coroutine_resume_wrapper);
    lua_setfield(L, -2, "__resume");

    lua_pushcfunction(L, lua_sched_coroutine_yield_wrapper);
    lua_setfield(L, -2, "__yield");

    lua_pushcfunction(L, lua_sched_coroutine_status_wrapper);
    lua_setfield(L, -2, "__status");

    /* Set as global coroutine table */
    lua_setglobal(L, "coroutine");

    /* Inject Lua code to dispatch between standard and custom coroutines */
    const char lua_code[] =
        "local keys = {'create', 'yield', 'resume', 'status', 'wrap'}\n"
        "for _, key in ipairs(keys) do\n"
        "  local std = coroutine['_' .. key]\n"
        "  local custom = coroutine['__' .. key]\n"
        "  coroutine[key] = function(...)\n"
        "    if custom then\n"
        "      return custom(...)\n"
        "    else\n"
        "      return std(...)\n"
        "    end\n"
        "  end\n"
        "end\n"
        "package.loaded.coroutine = coroutine\n";

    if (luaL_loadbuffer(L, lua_code, sizeof(lua_code) - 1, "=coroutine_api") != 0) {
        DLOG("Failed to load coroutine API: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }

    if (lua_pcall(L, 0, 0, 0) != 0) {
        DLOG("Failed to run coroutine API: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}