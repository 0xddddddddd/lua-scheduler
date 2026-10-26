#include "lua_scheduler.h"

#include <string.h>
#include <stdio.h>

#include "lua_uthread_api.h"
#include "lua_coroutine_api.h"

/* Forward declarations */
static int lua_sched_thread_spawn(lua_State* L);
static int lua_sched_thread_wait(lua_State* L);
static int lua_sched_thread_kill(lua_State* L);

/* thread.spawn - spawn a new user thread */
static int lua_sched_thread_spawn(lua_State* L)
{
    int n, co_ref;
    lua_sched_ctx* ctx;
    lua_sched_co_ctx* coctx = NULL;

    n = lua_gettop(L);

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    /* Create the coroutine */
    lua_sched_coroutine_create(L, ctx, &coctx, &co_ref);

    /* Pass arguments to the new thread */
    if (n > 1) {
        lua_replace(L, 1);
        lua_xmove(L, coctx->co, n - 1);
        coctx->nargs = n - 1;  /* Record number of arguments */
    } else {
        coctx->nargs = 0;
    }

    coctx->co_ref = co_ref;
    coctx->is_uthread = 1;
    ctx->uthreads++;

    coctx->co_status = LUA_SCHED_CO_RUNNING;
    ctx->co_op = LUA_SCHED_USER_THREAD_RESUME;

    ctx->cur_co_ctx->thread_spawn_yielded = 1;

    if (lua_sched_post_thread(ctx, ctx->cur_co_ctx) != 0) {
        return luaL_error(L, "no memory");
    }

    coctx->parent_co_ctx = ctx->cur_co_ctx;
    ctx->cur_co_ctx = coctx;

    lua_sched_attach_co_ctx_to_L(ctx->L, coctx->co, coctx);

    return lua_yield(L, 1);
}

/* thread.wait - wait for user threads to finish */
static int lua_sched_thread_wait(lua_State* L)
{
    int i, nargs, nrets;
    lua_State* sub_co;
    lua_sched_ctx* ctx;
    lua_sched_co_ctx* coctx, * sub_coctx;

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    coctx = ctx->cur_co_ctx;
    if (coctx == NULL) {
        return luaL_error(L, "no current coroutine context");
    }

    nargs = lua_gettop(L);
    if (nargs == 0) {
        return luaL_error(L, "at least one thread should be specified");
    }

    for (i = 1; i <= nargs; i++) {
        sub_co = lua_tothread(L, i);
        if (!sub_co) {
            return luaL_argerror(L, i, "thread expected");
        }

        sub_coctx = lua_sched_get_co_ctx(sub_co, ctx);
        if (sub_coctx == NULL) {
            return luaL_error(L, "no co ctx found");
        }

        if (!sub_coctx->is_uthread) {
            return luaL_error(L, "attempt to wait on a coroutine that is "
                              "not a user thread");
        }

        if (sub_coctx->parent_co_ctx != coctx) {
            return luaL_error(L, "only the parent coroutine can wait on the "
                              "thread");
        }

        switch (sub_coctx->co_status) {
            case LUA_SCHED_CO_ZOMBIE:
                /* Found zombie child - collect it */
                nrets = lua_gettop(sub_coctx->co);

                if (nrets) {
                    lua_xmove(sub_coctx->co, L, nrets);
                }

                lua_sched_del_co_ctx(ctx, sub_coctx);
                ctx->uthreads--;

                return nrets;

            case LUA_SCHED_CO_DEAD:
                if (i < nargs) {
                    /* Not the last one, ignore it */
                    continue;
                }

                /* Being the last one */
                lua_pushnil(L);
                lua_pushliteral(L, "already waited or killed");
                return 2;

            default:
                /* Thread still alive */
                break;
        }

        sub_coctx->waited_by_parent = 1;

        /* Mark the thread as high priority in the queue and record who's waiting */
        if (sub_coctx->co_status == LUA_SCHED_CO_RUNNING) {
            /* Find this thread in the posted queue and mark it high priority */
            lua_sched_posted_thread* pt;
            for (pt = ctx->posted_threads; pt != NULL; pt = pt->next) {
                if (pt->co_ctx == sub_coctx) {
                    pt->high_priority = 1;
                    pt->waited_by = coctx;  /* Record who is waiting */
                    break;
                }
            }
        }
    }

    /* Record which thread we're waiting for (for reference) */
    if (nargs == 1) {
        sub_co = lua_tothread(L, 1);
        if (sub_co) {
            sub_coctx = lua_sched_get_co_ctx(sub_co, ctx);
            if (sub_coctx) {
                coctx->waiting_for_thread = sub_coctx;
            }
        }
    }

    return lua_yield(L, 0);
}

/* thread.kill - kill a user thread */
static int lua_sched_thread_kill(lua_State* L)
{
    lua_State* sub_co;
    lua_sched_ctx* ctx;
    lua_sched_co_ctx* sub_coctx;

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    sub_co = lua_tothread(L, 1);
    if (!sub_co) {
        return luaL_argerror(L, 1, "thread expected");
    }

    sub_coctx = lua_sched_get_co_ctx(sub_co, ctx);
    if (sub_coctx == NULL) {
        return luaL_error(L, "no co ctx found");
    }

    if (!sub_coctx->is_uthread) {
        return luaL_error(L, "attempt to kill a coroutine that is "
                          "not a user thread");
    }

    if (sub_coctx->co_status == LUA_SCHED_CO_DEAD) {
        lua_pushnil(L);
        lua_pushliteral(L, "already killed or waited");
        return 2;
    }

    /* Mark as dead and cleanup */
    sub_coctx->co_status = LUA_SCHED_CO_DEAD;

    if (sub_coctx->cleanup) {
        sub_coctx->cleanup(sub_coctx->data);
        sub_coctx->cleanup = NULL;
    }

    if (sub_coctx->co_ref != LUA_NOREF) {
        lua_sched_del_co_ctx(ctx, sub_coctx);
        ctx->uthreads--;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Inject thread API into Lua state */
void lua_sched_inject_thread_api(lua_State* L)
{
    /* Create thread table */
    lua_createtable(L, 0, 3);

    lua_pushcfunction(L, lua_sched_thread_spawn);
    lua_setfield(L, -2, "spawn");

    lua_pushcfunction(L, lua_sched_thread_wait);
    lua_setfield(L, -2, "wait");

    lua_pushcfunction(L, lua_sched_thread_kill);
    lua_setfield(L, -2, "kill");

    /* Set as global thread table */
    lua_setglobal(L, "thread");
}