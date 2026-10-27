#include "lua_sleep_api.h"

#include <assert.h>

#include "lua_scheduler.h"
#include "lua_list.h"

void lua_sched_sleep_resume(lua_sched_ctx* ctx, lua_sched_co_ctx* coctx)
{
    if (ctx == NULL || coctx == NULL) {
        return;
    }

    //ctx->evs--;

    if (ctx->exited == 1) {
        return;
    }

    /* Clear references in coctx */
    coctx->data = NULL;
    coctx->cleanup = NULL;

    /* Mark coroutine as running so it will be resumed when scheduler resumes */
    coctx->co_status = LUA_SCHED_CO_RUNNING;

    if (ctx->paused) {
        lua_sched_posted_thread* pt = (lua_sched_posted_thread*)calloc(1, sizeof(lua_sched_posted_thread));
        if (pt != NULL) {
            pt->co_ctx = coctx;
            pt->high_priority = 0;
            pt->waited_by = NULL;
            slist_ptr_add_head(&ctx->paused_threads, pt);
            DLOG("Coroutine saved to paused_threads queue\n");
        }

        return;
    }

    /* Set as current coroutine and mark as running */
    ctx->cur_co_ctx = coctx;

    /* Resume the coroutine */
    int rc = lua_sched_run_thread(ctx, 0);
    /* Handle result and run posted threads */
    if (rc == SCHED_PAUSED) {
        /* Scheduler is paused, don't run posted threads */
        DLOG("Sleep timer fired but scheduler is paused\n");
        return;
    }

    /* Handle result and run posted threads */
    if (rc != SCHED_ERROR) {
        /* Run any posted threads (user threads spawned during execution) */
        rc = lua_sched_run_posted_threads(ctx->L, ctx);
        assert(rc == 0);
    }
}

static int lua_sched_sleep(lua_State *L)
{
    lua_sched_ctx* ctx;
    lua_sched_co_ctx* coctx = NULL;

    lua_Number delay_sec = luaL_checknumber(L, 1);
    if (delay_sec < 0) {
        return luaL_error(L, "invalid sleep duration \"%.3f\"", delay_sec);
    }

    uint64_t delay_ms = (uint64_t)(delay_sec * 1000);

    ctx = lua_registry_get(L);
    if (ctx == NULL) {
        return luaL_error(L, "no scheduler context found");
    }

    coctx = ctx->cur_co_ctx;
    if (coctx == NULL) {
        return luaL_error(L, "no co ctx found");
    }

    if (ctx->event.create == NULL) {
        return luaL_error(L, "sleep not supported");
    }

    //coctx->is_utimer = 1;
    /* Suspend coroutine */
    coctx->co_status = LUA_SCHED_CO_SUSPENDED;

    /* Create the timer - this should post the coroutine back when it fires */
    ctx->event.data = ctx->data;
    ctx->event.handler = lua_sched_sleep_resume;
    ctx->event.create(ctx, coctx, delay_ms);

    //ctx->evs++;

    return lua_yield(L, 0);
}

void lua_sched_inject_sleep_api(lua_State *L)
{
    lua_pushcfunction(L, lua_sched_sleep);
    lua_setglobal(L, "sleep");
}