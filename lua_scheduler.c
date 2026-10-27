#include "lua_scheduler.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lua_list.h"
#include "lua_coroutine_api.h"
#include "lua_uthread_api.h"
#include "lua_sleep_api.h"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void lua_sched_init_registry(lua_State* L);
static int lua_sched_atpanic(lua_State* L);
static int lua_sched_thread_traceback(lua_State* L, lua_State* co);
static void lua_sched_cleanup_zombie_child_threads(lua_sched_ctx* ctx,
                                                   lua_sched_co_ctx* coctx);
static int lua_sched_post_zombie_thread(lua_sched_co_ctx* parent,
                                        lua_sched_co_ctx* thread);

static void lua_sched_finalize_threads(lua_sched_ctx* ctx, lua_State* L);

/* ============================================================================
 * Registry Initialization
 * ============================================================================ */

static void lua_sched_init_registry(lua_State* L)
{
    /* Register a table to anchor Lua coroutines: {([int]ref) = [cort]} */
    lua_pushlightuserdata(L, lua_sched_lightudata_mask(coroutines_key));
    lua_createtable(L, 0, 32);
    lua_rawset(L, LUA_REGISTRYINDEX);

    /* Register a table to store coroutine contexts */
    lua_pushlightuserdata(L, lua_sched_lightudata_mask(co_ctx_key));
    lua_createtable(L, 0, 32);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

lua_sched_ctx* lua_sched_init(lua_State* L,
                              lua_sched_event* event,
                              void* data)
{
    lua_sched_ctx* ctx;

    if (L == NULL) {
        DLOG("lua_sched_init: NULL Lua state\n");
        return NULL;
    }

    ctx = (lua_sched_ctx*)calloc(1, sizeof(lua_sched_ctx));
    if (ctx == NULL) {
        DLOG("lua_sched_init: out of memory\n");
        return NULL;
    }
    ctx->data = data;
    ctx->L = L;
    ctx->cur_co_ctx = NULL;
    ctx->posted_threads = NULL;
    ctx->paused_threads = NULL;
    ctx->co_op = LUA_SCHED_USER_CORO_NOP;
    ctx->uthreads = 0;
    ctx->exited = 0;

    /* Initialize entry coroutine context */
    memset(&ctx->entry_co_ctx, 0, sizeof(lua_sched_co_ctx));
    ctx->entry_co_ctx.next_zombie_child_thread =
        &ctx->entry_co_ctx.zombie_child_threads;
    ctx->entry_co_ctx.co_ref = LUA_NOREF;
    ctx->entry_co_ctx.co_status = LUA_SCHED_CO_SUSPENDED;
    ctx->entry_co_ctx.co_entry = 1;
    ctx->entry_co_ctx.index = ~0;

    ctx->co_ctx_bucket = list_ctor();

    ctx->event.data = event->data;
    ctx->event.create = event->create;
    ctx->event.handler = lua_sched_sleep_resume;
    ctx->event.complete = event->complete;

    lua_main_set(L, ctx);
    /* Initialize Lua registry for coroutines */
    lua_sched_init_registry(L);

    /* Store context in registry */
    lua_registry_set(L, ctx);

    /* Inject coroutine and thread APIs */
    lua_sched_inject_coroutine_api(L);
    lua_sched_inject_thread_api(L);
    lua_sched_inject_sleep_api(L);

    return ctx;
}

/* ============================================================================
 * Scheduler Cleanup
 * ============================================================================ */

void lua_sched_cleanup(lua_sched_ctx* ctx)
{
    struct list* pos, *n;

    if (ctx == NULL) {
        return;
    }

    lua_sched_finalize_threads(ctx, ctx->L);

    if (ctx->co_ctx_bucket) {
        list_for_each_safe(pos, n, ctx->co_ctx_bucket)
        {
            lua_sched_co_ctx* node = list_entry(pos, lua_sched_co_ctx, link);

            if (node->zombie_child_threads) {
                lua_sched_cleanup_zombie_child_threads(ctx, node);
            }

            assert(node->co_ref == LUA_NOREF);
            DLOG("Freeing co_ctx node=%p, co_status=%d, co_ref=%d\n",
                 node, node->co_status, node->co_ref);
            free(node);
        }

        free(ctx->co_ctx_bucket);
    }

    free(ctx);
}

static int lua_sched_atpanic(lua_State* L)
{
    const char* msg = lua_tostring(L, -1);
    DLOG("PANIC: unprotected error in call to Lua API (%s)\n",
         msg ? msg : "no message");
    return 0;
}

/* ============================================================================
 * Thread Management (Lua Threads)
 * ============================================================================ */

lua_State* lua_sched_new_thread(lua_State* L, int* ref)
{
    lua_State* co;
    int base;

    base = lua_gettop(L);

    lua_pushlightuserdata(L, lua_sched_lightudata_mask(coroutines_key));
    lua_rawget(L, LUA_REGISTRYINDEX);

    co = lua_newthread(L);

    /* The coroutines table is at -2, the new thread is at -1 */
    *ref = luaL_ref(L, -2);

    if (*ref == LUA_NOREF) {
        lua_settop(L, base);
        return NULL;
    }

    lua_settop(L, base);
    return co;
}

void lua_sched_del_thread(lua_State* L, int co_ref)
{
    if (co_ref == LUA_NOREF) {
        return;
    }

    lua_pushlightuserdata(L, lua_sched_lightudata_mask(coroutines_key));
    lua_rawget(L, LUA_REGISTRYINDEX);
    luaL_unref(L, -1, co_ref);
    lua_pop(L, 1);
}

/* ============================================================================
 * Pause/Resume Control
 * ============================================================================ */

void lua_sched_pause(lua_sched_ctx* ctx)
{
    if (ctx == NULL) {
        return;
    }

    DLOG("Pausing scheduler\n");
    ctx->paused = 1;
}

void lua_sched_resume(lua_sched_ctx* ctx)
{
    lua_sched_posted_thread* pt;

    if (ctx == NULL) {
        return;
    }

    DLOG("Resuming scheduler\n");
    ctx->paused = 0;

    /* If there's a current coroutine that was suspended, mark it as running */
    if (ctx->cur_co_ctx && ctx->cur_co_ctx->co_status == LUA_SCHED_CO_SUSPENDED) {
        ctx->cur_co_ctx->co_status = LUA_SCHED_CO_RUNNING;
    }

    /* Move all paused threads back to posted_threads queue */
    if (ctx->paused_threads) {
        DLOG("Restoring %d paused threads to execution queue\n",
             ctx->paused_threads ? 1 : 0);

        /* Find the end of paused_threads list */
        pt = ctx->paused_threads;
        while (pt->next != NULL) {
            pt = pt->next;
        }

        /* Append posted_threads to the end of paused_threads */
        pt->next = ctx->posted_threads;

        /* Make paused_threads the new posted_threads */
        ctx->posted_threads = ctx->paused_threads;
        ctx->paused_threads = NULL;

        DLOG("All paused threads restored to execution queue\n");
    }
}

int lua_sched_is_paused(lua_sched_ctx* ctx)
{
    if (ctx == NULL) {
        return 0;
    }

    return ctx->paused;
}

int lua_sched_is_all_done(lua_sched_ctx* ctx)
{
    if (ctx == NULL) {
        return 1;
    }

    /* Check if entry coroutine is dead */
    if (ctx->entry_co_ctx.co_status != LUA_SCHED_CO_DEAD) {
        return 0;
    }

    /* Check if there are active user threads */
    if (ctx->uthreads > 0) {
        return 0;
    }

    /*if (list_count_nodes(ctx->co_ctx_bucket) > 0) {
        return 0;
    }*/

    /* Check if there are threads in posted queue */
    if (ctx->posted_threads != NULL) {
        return 0;
    }

    /* Check if there are threads in paused queue */
    if (ctx->paused_threads != NULL) {
        return 0;
    }

    return 1;
}

/* ============================================================================
 * Scheduler Exit/Stop Control
 * ============================================================================ */

void lua_sched_exit(lua_sched_ctx* ctx)
{
    ctx->exited = 1;
}

int lua_sched_stopped(lua_sched_ctx* ctx)
{
    return ctx->exited;
}

/* ============================================================================
 * Queue Cleanup Functions
 * ============================================================================ */

void lua_sched_posted_threads_cleanup(lua_sched_ctx* ctx)
{
    lua_sched_posted_thread *prev, *pos, *n;
    // 3. 清理 posted_threads 队列中的协程引用
    slist_ptr_for_each_safe(ctx->posted_threads, prev, pos, n) {
        if (pos->co_ctx && pos->co_ctx->co_ref != LUA_NOREF) {
            lua_sched_del_co_ctx(ctx, pos->co_ctx);
        }
        free(pos);
    }
    ctx->posted_threads = NULL;
}

void lua_sched_paused_threads_cleanup(lua_sched_ctx* ctx)
{
    lua_sched_posted_thread *prev, *pos, *n;
    // 3. 清理 posted_threads 队列中的协程引用
    slist_ptr_for_each_safe(ctx->paused_threads, prev, pos, n) {
        if (pos->co_ctx && pos->co_ctx->co_ref != LUA_NOREF) {
            lua_sched_del_co_ctx(ctx, pos->co_ctx);
        }
        ctx->evs--;
        free(pos);
    }
    ctx->paused_threads = NULL;
}

void lua_sched_co_ctx_bucket_cleanup(lua_sched_ctx* ctx)
{
    if (ctx->co_ctx_bucket) {
        struct list* pos, *next;
        list_for_each_safe(pos, next, ctx->co_ctx_bucket) {
            lua_sched_co_ctx* node = list_entry(pos, lua_sched_co_ctx, link);

            // 清理僵尸子线程
            if (node->zombie_child_threads) {
                lua_sched_cleanup_zombie_child_threads(ctx, node);
            }

            // 释放协程引用
            if (node->co_ref != LUA_NOREF) {
                lua_sched_del_co_ctx(ctx, node);
            }

            if (node->is_uthread) {
                ctx->uthreads--;
            }
        }
    }
}


void lua_sched_force_stop(lua_sched_ctx* ctx)
{
    if (ctx == NULL) return;

    // 1. 设置退出标志
    ctx->exited = 1;
    ctx->paused = 1;

    // 2. 清理当前协程
    if (ctx->cur_co_ctx && ctx->cur_co_ctx != &ctx->entry_co_ctx) {
        if (ctx->cur_co_ctx->co_ref != LUA_NOREF) {
            lua_sched_del_co_ctx(ctx, ctx->cur_co_ctx);
        }
        ctx->cur_co_ctx = NULL;
    }

    // 3. 清理 posted_threads 队列中的协程引用
    lua_sched_posted_threads_cleanup(ctx);
    // 4. 清理 paused_threads 队列中的协程引用
    lua_sched_paused_threads_cleanup(ctx);
    // 5. 清理 bucket 中的所有用户协程
    lua_sched_co_ctx_bucket_cleanup(ctx);
    // 6. 清理 entry 协程
    if (ctx->entry_co_ctx.co_ref != LUA_NOREF) {
        lua_sched_del_co_ctx(ctx, &ctx->entry_co_ctx);
    }

    if (lua_sched_is_all_done(ctx)) {
        if (ctx->event.complete) {
            ctx->event.complete(ctx, ctx->data);
        }
        DLOG("All threads completed 2\n");
    }
}

lua_sched_co_ctx* lua_sched_create_co_ctx(lua_sched_ctx* ctx)
{
    lua_sched_co_ctx* coctx;

    coctx = (lua_sched_co_ctx*)calloc(1, sizeof(lua_sched_co_ctx));
    if (coctx == NULL) {
        return NULL;
    }

    coctx->next_zombie_child_thread = &coctx->zombie_child_threads;
    coctx->co_ref = LUA_NOREF;
    coctx->co_status = LUA_SCHED_CO_SUSPENDED;
    coctx->index = 0;
    list_init(&coctx->link);
    //coctx->sleep = (lua_sched_co_sleep*)calloc(1, sizeof(lua_sched_co_sleep));

    list_add(ctx->co_ctx_bucket, &coctx->link);

    coctx->index = list_count_nodes(ctx->co_ctx_bucket);

    return coctx;
}

lua_sched_co_ctx* lua_sched_get_co_ctx(lua_State* co, lua_sched_ctx* ctx)
{
    lua_sched_co_ctx* coctx;

    lua_pushlightuserdata(ctx->L, lua_sched_lightudata_mask(co_ctx_key));
    lua_rawget(ctx->L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(ctx->L, co);
    lua_rawget(ctx->L, -2);

    coctx = (lua_sched_co_ctx*)lua_touserdata(ctx->L, -1);
    lua_pop(ctx->L, 2);

    return coctx;
}


void lua_sched_del_co_ctx(lua_sched_ctx* ctx, lua_sched_co_ctx* co_ctx)
{
    if (co_ctx->co_ref == LUA_NOREF) {
        return;
    }

    lua_sched_del_thread(ctx->L, co_ctx->co_ref);
    co_ctx->co_ref = LUA_NOREF;
    co_ctx->co_status = LUA_SCHED_CO_DEAD;
}

void lua_sched_del_co_ctx_free(lua_sched_ctx* ctx, lua_sched_co_ctx* co_ctx)
{
    if (co_ctx == NULL) {
        return;
    }
    /* Don't free entry_co_ctx as it's embedded in lua_sched_ctx */
    if (co_ctx != &ctx->entry_co_ctx) {
        free(co_ctx);
    }
}

/* ============================================================================
 * Thread Posting and Zombie Management
 * ============================================================================ */

int lua_sched_post_thread(lua_sched_ctx* ctx, lua_sched_co_ctx* co_ctx)
{
    lua_sched_posted_thread* pt;

    pt = (lua_sched_posted_thread*)malloc(sizeof(lua_sched_posted_thread));
    if (pt == NULL) {
        return -1;
    }

    pt->co_ctx = co_ctx;
    pt->next = ctx->posted_threads;
    pt->high_priority = 0;
    pt->waited_by = NULL;

    /* If this thread is waited by parent, preserve that information */
    if (co_ctx->waited_by_parent && co_ctx->parent_co_ctx) {
        pt->high_priority = 1;
        pt->waited_by = co_ctx->parent_co_ctx;
    }

    ctx->posted_threads = pt;
    return 0;
}

static int lua_sched_post_zombie_thread(lua_sched_co_ctx* parent,
                                        lua_sched_co_ctx* thread)
{
    lua_sched_posted_thread* pt;

    pt = (lua_sched_posted_thread*)malloc(sizeof(lua_sched_posted_thread));
    if (pt == NULL) {
        return -1;
    }

    pt->co_ctx = thread;
    pt->next = NULL;
    pt->high_priority = 0;
    pt->waited_by = NULL;

    *parent->next_zombie_child_thread = pt;
    parent->next_zombie_child_thread = &pt->next;

    return 0;
}

static void lua_sched_cleanup_zombie_child_threads(lua_sched_ctx* ctx,
                                                   lua_sched_co_ctx* coctx)
{
    lua_sched_co_ctx* child_coctx;
    lua_sched_posted_thread *prev, *pos, *n;

    slist_ptr_for_each_safe(coctx->zombie_child_threads, prev, pos, n) {
        child_coctx = pos->co_ctx;

        if (child_coctx && child_coctx->co_ref != LUA_NOREF) {
            lua_sched_del_co_ctx(ctx, child_coctx);
            ctx->uthreads--;
        }

        free(pos);
    }

    /* reset */
    coctx->zombie_child_threads = NULL;
    coctx->next_zombie_child_thread = &coctx->zombie_child_threads;
}

static void lua_sched_finalize_threads(lua_sched_ctx* ctx, lua_State* L)
{
    lua_sched_co_ctx* coctx;
    lua_sched_posted_thread* prev, *pos, *n;
    /* Clean up entry thread */
    coctx = &ctx->entry_co_ctx;
    if (coctx->co_ref != LUA_NOREF) {
        lua_sched_del_co_ctx(ctx, coctx);
    }

    if (coctx->zombie_child_threads) {
        lua_sched_cleanup_zombie_child_threads(ctx, coctx);
    }

    /* Clear posted threads */
    slist_ptr_for_each_safe(ctx->posted_threads, prev, pos, n) {
        free(pos);
    }

    /* Clear paused threads */
    slist_ptr_for_each_safe(ctx->paused_threads, prev, pos, n) {
        free(pos);
    }
}

/* ============================================================================
 * Error Handling and Traceback
 * ============================================================================ */

static int lua_sched_thread_traceback(lua_State* L, lua_State* co)
{
    const char* msg;

    if (!lua_isstring(co, -1)) {
        return 0;
    }

    msg = lua_tostring(co, -1);
    luaL_traceback(L, co, msg, 1);

    return 1;
}

/* ============================================================================
 * Coroutine Creation
 * ============================================================================ */

int lua_sched_coroutine_create(lua_State* L, lua_sched_ctx* ctx,
                               lua_sched_co_ctx** pcoctx, int* co_ref)
{
    lua_State* co;
    lua_sched_co_ctx* coctx;
    int ref;
    int base;

    if (!lua_isfunction(L, 1) || lua_iscfunction(L, 1)) {
        luaL_error(L, "Lua function expected");
        return 0;
    }

    base = lua_gettop(ctx->L);

    /* Create new coroutine on main Lua state */
    co = lua_sched_new_thread(ctx->L, &ref);
    if (co == NULL) {
        luaL_error(L, "failed to create thread");
        return 0;
    }

    if (co_ref) {
        *co_ref = ref;
    }

    /* Get or create coroutine context */
    coctx = lua_sched_get_co_ctx(co, ctx);
    if (coctx == NULL) {
        coctx = lua_sched_create_co_ctx(ctx);
        if (coctx == NULL) {
            lua_sched_del_thread(ctx->L, ref);
            luaL_error(L, "no memory");
            return 0;
        }
    } else {
        memset(coctx, 0, sizeof(lua_sched_co_ctx));
        coctx->next_zombie_child_thread = &coctx->zombie_child_threads;
        coctx->co_ref = LUA_NOREF;
    }

    coctx->co = co;
    coctx->co_status = LUA_SCHED_CO_SUSPENDED;

    /* Attach context to coroutine */
    lua_sched_attach_co_ctx_to_L(ctx->L, co, coctx);

    /* Push the coroutine thread to L's stack */
    lua_pushlightuserdata(ctx->L, lua_sched_lightudata_mask(coroutines_key));
    lua_rawget(ctx->L, LUA_REGISTRYINDEX);
    lua_rawgeti(ctx->L, -1, ref);
    lua_xmove(ctx->L, L, 1);
    lua_pop(ctx->L, 1);

    /* Restore main VM stack */
    lua_settop(ctx->L, base);

    /* Copy entry function to coroutine */
    lua_pushvalue(L, 1);
    lua_xmove(L, co, 1);

    if (pcoctx) {
        *pcoctx = coctx;
    }

    coctx->co_top = 1;
    return 1;
}

/* ============================================================================
 * Posted Thread Runner
 * ============================================================================ */

int lua_sched_run_posted_threads(lua_State* L, lua_sched_ctx* ctx)
{
    lua_sched_posted_thread* pt;
    lua_sched_posted_thread* prev;
    lua_sched_posted_thread* high_pri_pt;
    lua_sched_posted_thread* high_pri_prev;
    int rc;

    DLOG("DEBUG: lua_sched_run_posted_threads called\n");

    /* First pass: look for high priority threads */
    high_pri_pt = NULL;
    high_pri_prev = NULL;
    prev = NULL;

    for (pt = ctx->posted_threads; pt != NULL; prev = pt, pt = pt->next) {
        DLOG("  Queue entry: co_ctx=%p, high_priority=%d, waited_by=%p, status=%d\n",
             pt->co_ctx, pt->high_priority, pt->waited_by, pt->co_ctx->co_status);

        if (pt->high_priority && pt->co_ctx->co_status == LUA_SCHED_CO_RUNNING) {
            high_pri_pt = pt;
            high_pri_prev = prev;
            DLOG("  Found high priority thread (waited by %p)!\n", pt->waited_by);
            break;
        }
    }

    /* If we found a high priority thread, run it first */
    if (high_pri_pt) {
        DLOG("DEBUG: Running high priority thread co_ctx=%p\n", high_pri_pt->co_ctx);

        /* Remove from queue */
        if (high_pri_prev) {
            high_pri_prev->next = high_pri_pt->next;
        } else {
            ctx->posted_threads = high_pri_pt->next;
        }

        ctx->cur_co_ctx = high_pri_pt->co_ctx;
        free(high_pri_pt);

        rc = lua_sched_run_thread(ctx, 0);

        /* If thread yielded or finished, continue processing other threads */
        if (rc == SCHED_AGAIN || rc == SCHED_DONE) {
            DLOG("  High priority thread yielded/finished, processing other threads\n");
            /* Fall through to normal processing */
        } else if (rc == SCHED_PAUSED) {
            DLOG("  Scheduler paused\n");
            return rc;
        } else {
            return rc;
        }
    }

    /* Normal queue processing */
    DLOG("DEBUG: Processing posted threads normally\n");

    while (1) {
        pt = ctx->posted_threads;
        if (pt == NULL) {
            DLOG("  No more threads in queue\n");
            if (lua_sched_is_all_done(ctx)) {
                if (ctx->event.complete) {
                    ctx->event.complete(ctx, ctx->data);
                }
                DLOG("All threads completed 1\n");
            }
            return 0;
        }

        ctx->posted_threads = pt->next;

        if (pt->co_ctx->co_status != LUA_SCHED_CO_RUNNING) {
            free(pt);
            continue;
        }

        /* If there are high-priority threads waiting, skip non-priority threads */
        if (!pt->high_priority) {
            lua_sched_posted_thread* check_pt;
            int has_high_pri = 0;

            for (check_pt = ctx->posted_threads; check_pt != NULL; check_pt = check_pt->next) {
                if (check_pt->high_priority && check_pt->co_ctx->co_status == LUA_SCHED_CO_RUNNING) {
                    has_high_pri = 1;
                    break;
                }
            }

            if (has_high_pri) {
                DLOG("  Skipping non-priority thread, putting back in queue\n");
                /* Put this thread back at the end of the queue */
                lua_sched_posted_thread** tail = &ctx->posted_threads;
                while (*tail != NULL) {
                    tail = &(*tail)->next;
                }
                pt->next = NULL;
                *tail = pt;
                continue;
            }
        }

        DLOG("  Running normal thread co_ctx=%p (waited_by=%p, high_pri=%d)\n",
             pt->co_ctx, pt->waited_by, pt->high_priority);
        ctx->cur_co_ctx = pt->co_ctx;
        free(pt);

        rc = lua_sched_run_thread(ctx, 0);

        if (rc == SCHED_AGAIN || rc == SCHED_DONE) {
            continue;
        } else if (rc == SCHED_PAUSED) {
            DLOG("  Scheduler paused\n");
            return rc;
        } else {
            return rc;
        }

        return rc;
    }
}

static int lua_sched_handle_coro_nop(lua_sched_ctx* ctx)
{
    ctx->cur_co_ctx = NULL;
    return SCHED_AGAIN;
}

static int lua_sched_handle_thread_resume(lua_State* L, lua_sched_ctx* ctx, int* nrets)
{
    lua_sched_co_ctx* parent_coctx;

    DLOG("DEBUG: USER_THREAD_RESUME - parent spawned a thread\n");
    DLOG("  Current ctx->cur_co_ctx=%p (should be child)\n", ctx->cur_co_ctx);
    DLOG("  Parent co_ctx=%p\n", ctx->cur_co_ctx->parent_co_ctx);

    ctx->co_op = LUA_SCHED_USER_CORO_NOP;

    parent_coctx = ctx->cur_co_ctx->parent_co_ctx;
    if (parent_coctx == NULL) {
        return luaL_error(L, "thread has no parent");
    }

    DLOG("  Child co=%p, stack top=%d\n",
         ctx->cur_co_ctx->co, lua_gettop(ctx->cur_co_ctx->co));

    if (lua_sched_post_thread(ctx, ctx->cur_co_ctx) != 0) {
        return SCHED_ERROR;
    }

    ctx->cur_co_ctx = parent_coctx;
    parent_coctx->co_status = LUA_SCHED_CO_RUNNING;

    *nrets = 1;
    DLOG("  Parent co=%p, stack top=%d, continuing with nrets=%d\n",
         parent_coctx->co, lua_gettop(parent_coctx->co), *nrets);

    ctx->cur_co_ctx->co_top--;

    DLOG("  Continuing parent coroutine\n");
    return SCHED_CONTINUE;
}

static int lua_sched_handle_coro_resume(lua_sched_ctx* ctx, int* nrets)
{
    lua_State* old_co;

    DLOG("DEBUG: USER_CORO_RESUME - switching to child coroutine\n");

    old_co = ctx->cur_co_ctx->parent_co_ctx->co;

    DLOG("  Parent co=%p, top=%d\n", old_co, lua_gettop(old_co));

    /* Debug: print parent stack contents */
    for (int i = 1; i <= lua_gettop(old_co); i++) {
        int t = lua_type(old_co, i);
        DLOG("    [%d] = %s", i, lua_typename(old_co, t));
        if (t == LUA_TSTRING) {
            DLOG(" (%s)", lua_tostring(old_co, i));
        } else if (t == LUA_TNUMBER) {
            DLOG(" (%g)", lua_tonumber(old_co, i));
        }
        DLOG("\n");
    }

    DLOG("  Child co=%p, top=%d\n", ctx->cur_co_ctx->co, lua_gettop(ctx->cur_co_ctx->co));

    ctx->co_op = LUA_SCHED_USER_CORO_NOP;

    /* Remove thread from parent stack */
    if (lua_gettop(old_co) > 0 && lua_isthread(old_co, 1)) {
        lua_remove(old_co, 1);
        ctx->cur_co_ctx->parent_co_ctx->co_top--;
    }

    *nrets = lua_gettop(old_co);

    DLOG("  After removing thread: parent top=%d\n", *nrets);
    DLOG("  Moving %d arguments from parent to child\n", *nrets);

    if (*nrets > 0) {
        lua_xmove(old_co, ctx->cur_co_ctx->co, *nrets);
        ctx->cur_co_ctx->parent_co_ctx->co_top -= *nrets;
    }

    DLOG("  Child co top after xmove=%d\n", lua_gettop(ctx->cur_co_ctx->co));

    /* Debug: print child stack contents */
    for (int i = 1; i <= lua_gettop(ctx->cur_co_ctx->co); i++) {
        int t = lua_type(ctx->cur_co_ctx->co, i);
        DLOG("    [%d] = %s", i, lua_typename(ctx->cur_co_ctx->co, t));
        if (t == LUA_TSTRING) {
            DLOG(" (%s)", lua_tostring(ctx->cur_co_ctx->co, i));
        } else if (t == LUA_TNUMBER) {
            DLOG(" (%g)", lua_tonumber(ctx->cur_co_ctx->co, i));
        }
        DLOG("\n");
    }

    DLOG("  Will resume child with nrets=%d\n", *nrets);
    return SCHED_CONTINUE;
}

static int lua_sched_handle_coro_yield(lua_sched_ctx* ctx, int* nrets)
{
    lua_sched_co_ctx* next_coctx;
    lua_State* next_co;

    DLOG("DEBUG: USER_CORO_YIELD (default) - child yielded\n");
    ctx->co_op = LUA_SCHED_USER_CORO_NOP;

    if (lua_sched_is_thread(ctx)) {
        DLOG("  User thread yielded, posting back to queue\n");
        lua_settop(ctx->cur_co_ctx->co, 0);
        ctx->cur_co_ctx->co_top = 0;
        ctx->cur_co_ctx->co_status = LUA_SCHED_CO_RUNNING;

        if (ctx->posted_threads) {
            DLOG("  Posting thread and continuing with other threads\n");
            lua_sched_post_thread(ctx, ctx->cur_co_ctx);
            ctx->cur_co_ctx = NULL;
            return SCHED_AGAIN;
        }

        DLOG("  No other threads, continuing with this one\n");
        *nrets = 0;
        return SCHED_CONTINUE;
    }

    if (lua_sched_is_entry_thread(ctx)) {
        DLOG("  Entry thread yielded, suspending and checking posted threads\n");
        ctx->cur_co_ctx->co_status = LUA_SCHED_CO_SUSPENDED;
        ctx->cur_co_ctx = NULL;
        return SCHED_AGAIN;
    }

    *nrets = lua_gettop(ctx->cur_co_ctx->co);
    next_coctx = ctx->cur_co_ctx->parent_co_ctx;

    if (next_coctx == NULL) {
        DLOG("  Entry thread yielded, suspending and checking posted threads\n");
        ctx->cur_co_ctx->co_status = LUA_SCHED_CO_SUSPENDED;
        ctx->cur_co_ctx = NULL;
        return SCHED_AGAIN;
    }

    next_co = next_coctx->co;

    DLOG("  Child co=%p has %d values on stack\n", ctx->cur_co_ctx->co, *nrets);

    /* Debug child stack before move */
    for (int i = 1; i <= *nrets; i++) {
        DLOG("    Child[%d] = %s\n", i, lua_typename(ctx->cur_co_ctx->co, lua_type(ctx->cur_co_ctx->co, i)));
    }

    DLOG("  Parent co=%p top before move: %d\n", next_co, lua_gettop(next_co));

    if (*nrets) {
        lua_xmove(ctx->cur_co_ctx->co, next_co, *nrets);
        ctx->cur_co_ctx->co_top -= *nrets;
    }

    DLOG("  Parent co top after xmove: %d\n", lua_gettop(next_co));

    if (!ctx->cur_co_ctx->is_wrap) {
        lua_pushboolean(next_co, 1);
        lua_insert(next_co, 1);
        (*nrets)++;
        DLOG("  Added 'true' to front, parent now has %d values\n", lua_gettop(next_co));
    }

    /* Debug parent stack after all moves */
    for (int i = 1; i <= lua_gettop(next_co); i++) {
        DLOG("    Parent[%d] = %s\n", i, lua_typename(next_co, lua_type(next_co, i)));
    }

    ctx->cur_co_ctx = next_coctx;
    next_coctx->co_status = LUA_SCHED_CO_RUNNING;

    DLOG("  Switching to parent, will continue loop with nrets=%d\n", *nrets);

    next_coctx->co_top = *nrets;
    return SCHED_CONTINUE;
}

static int lua_sched_handle_coroutine_operation(lua_State* L, lua_sched_ctx* ctx, int* nrets)
{
    switch (ctx->co_op) {
        case LUA_SCHED_USER_CORO_NOP:
            return lua_sched_handle_coro_nop(ctx);

        case LUA_SCHED_USER_THREAD_RESUME:
            return lua_sched_handle_thread_resume(L, ctx, nrets);

        case LUA_SCHED_USER_CORO_RESUME:
            return lua_sched_handle_coro_resume(ctx, nrets);

        default:
            return lua_sched_handle_coro_yield(ctx, nrets);
    }
}

static int lua_sched_handle_entry_thread_done(lua_sched_ctx* ctx, lua_State* L)
{
    lua_settop(L, 0);
    lua_sched_del_co_ctx(ctx, ctx->cur_co_ctx);

    if (ctx->uthreads) {
        ctx->cur_co_ctx = NULL;
        return SCHED_AGAIN;
    }

    return SCHED_DONE;
}

static int lua_sched_handle_user_thread_done(lua_sched_ctx* ctx, lua_State* L, int* nrets)
{
    lua_sched_co_ctx* parent_coctx;
    lua_State* next_co;

    DLOG("DEBUG: User thread finished, is_uthread=1\n");
    parent_coctx = ctx->cur_co_ctx->parent_co_ctx;
    DLOG("  Parent coctx=%p\n", parent_coctx);

    if (lua_sched_coroutine_alive(parent_coctx)) {
        DLOG("  Parent is alive, waited_by_parent=%d\n",
             ctx->cur_co_ctx->waited_by_parent);

        if (ctx->cur_co_ctx->waited_by_parent) {
            ctx->cur_co_ctx->waited_by_parent = 0;

            /* Clear parent's waiting_for_thread */
            if (parent_coctx->waiting_for_thread == ctx->cur_co_ctx) {
                DLOG("  Clearing parent's waiting_for_thread\n");
                parent_coctx->waiting_for_thread = NULL;
            }

            /* Clear parent stack for user threads before returning values */
            DLOG("  Clearing parent stack (was %d)\n", lua_gettop(parent_coctx->co));
            lua_settop(parent_coctx->co, 0);

            /* Get return values from child */
            *nrets = lua_gettop(ctx->cur_co_ctx->co);
            next_co = parent_coctx->co;

            if (*nrets) {
                lua_xmove(ctx->cur_co_ctx->co, next_co, *nrets);
            }

            /* Delete user thread context */
            if (ctx->cur_co_ctx->co_ref != LUA_NOREF) {
                lua_sched_del_co_ctx(ctx, ctx->cur_co_ctx);
                ctx->uthreads--;
            }

            /* Switch to parent and continue */
            ctx->cur_co_ctx = parent_coctx;
            parent_coctx->co_status = LUA_SCHED_CO_RUNNING;

            DLOG("  Continuing parent with %d return values\n", *nrets);
            return SCHED_CONTINUE;
        }

        /* No one waiting, make this a zombie */
        DLOG("  No one waiting, making zombie\n");
        lua_settop(L, 0);

        if (lua_sched_post_zombie_thread(parent_coctx, ctx->cur_co_ctx) != 0) {
            return SCHED_ERROR;
        }

        lua_pushboolean(ctx->cur_co_ctx->co, 1);
        lua_insert(ctx->cur_co_ctx->co, 1);

        ctx->cur_co_ctx->co_status = LUA_SCHED_CO_ZOMBIE;
        ctx->cur_co_ctx = NULL;
        return SCHED_AGAIN;
    }

    lua_settop(L, 0);

    if (ctx->cur_co_ctx->co_ref != LUA_NOREF) {
        lua_sched_del_co_ctx(ctx, ctx->cur_co_ctx);
        ctx->uthreads--;
    }

    if (ctx->uthreads == 0) {
        if (lua_sched_entry_thread_alive(ctx)) {
            ctx->cur_co_ctx = NULL;
            return SCHED_AGAIN;
        }
        return SCHED_DONE;
    }

    ctx->cur_co_ctx = NULL;
    return SCHED_AGAIN;
}

static int lua_sched_handle_user_coroutine_done(lua_sched_ctx* ctx, int success, int* nrets)
{
    lua_sched_co_ctx* next_coctx;
    lua_State* next_co;

    DLOG("DEBUG: user coroutine done\n");
    *nrets = lua_gettop(ctx->cur_co_ctx->co);
    DLOG("  Child has %d return values\n", *nrets);

    next_coctx = ctx->cur_co_ctx->parent_co_ctx;

    if (next_coctx == NULL) {
        lua_settop(ctx->L, 0);
        ctx->cur_co_ctx->co_status = LUA_SCHED_CO_DEAD;
        lua_sched_finalize_threads(ctx, ctx->L);
        DLOG("lua handler aborted: user coroutine has no parent\n");
        return SCHED_ERROR;
    }

    next_co = next_coctx->co;

    if (*nrets) {
        lua_xmove(ctx->cur_co_ctx->co, next_co, *nrets);
        DLOG("  Moved %d values to parent\n", *nrets);
    }

    if (ctx->cur_co_ctx->is_uthread && ctx->cur_co_ctx->co_ref != LUA_NOREF) {
        lua_sched_del_co_ctx(ctx, ctx->cur_co_ctx);
        ctx->uthreads--;
        DLOG("  Deleted user thread context, uthreads=%d\n", ctx->uthreads);
    }

    if (!ctx->cur_co_ctx->is_wrap) {
        lua_pushboolean(next_co, success);
        lua_insert(next_co, 1);
        (*nrets)++;
        DLOG("  Added success boolean, total nrets=%d\n", *nrets);
    }

    ctx->cur_co_ctx = next_coctx;
    next_coctx->co_status = LUA_SCHED_CO_RUNNING;

    DLOG("  Continuing with parent, nrets=%d\n", *nrets);
    return SCHED_CONTINUE;
}

static int lua_sched_handle_coroutine_success(lua_sched_ctx* ctx, lua_State* L, int* nrets)
{
    ctx->cur_co_ctx->co_status = LUA_SCHED_CO_DEAD;

    if (ctx->cur_co_ctx->zombie_child_threads) {
        lua_sched_cleanup_zombie_child_threads(ctx, ctx->cur_co_ctx);
    }

    if (lua_sched_is_entry_thread(ctx)) {
        return lua_sched_handle_entry_thread_done(ctx, L);
    }

    if (ctx->cur_co_ctx->is_uthread) {
        return lua_sched_handle_user_thread_done(ctx, L, nrets);
    }

    return lua_sched_handle_user_coroutine_done(ctx, 1, nrets);
}

/* ============================================================================
 * Error Handling
 * ============================================================================ */

static int lua_sched_handle_user_thread_error(lua_sched_ctx* ctx, lua_State* L,
                                              const char* err, const char* msg, int* nrets)
{
    lua_sched_co_ctx* parent_coctx;

    DLOG("lua user thread aborted: %s: %s\n",
         err ? err : "error", msg ? msg : "no message");

    lua_settop(L, 0);
    parent_coctx = ctx->cur_co_ctx->parent_co_ctx;

    if (lua_sched_coroutine_alive(parent_coctx)) {
        if (ctx->cur_co_ctx->waited_by_parent) {
            ctx->cur_co_ctx->waited_by_parent = 0;

            /* Clear parent's waiting_for_thread */
            if (parent_coctx->waiting_for_thread == ctx->cur_co_ctx) {
                parent_coctx->waiting_for_thread = NULL;
            }

            /* For user thread errors, push error to parent */
            lua_pushnil(parent_coctx->co);
            lua_pushstring(parent_coctx->co, msg ? msg : "thread error");

            /* Delete user thread context */
            if (ctx->cur_co_ctx->co_ref != LUA_NOREF) {
                lua_sched_del_co_ctx(ctx, ctx->cur_co_ctx);
                ctx->uthreads--;
            }

            /* Switch to parent and continue */
            ctx->cur_co_ctx = parent_coctx;
            parent_coctx->co_status = LUA_SCHED_CO_RUNNING;

            *nrets = 2;
            return SCHED_CONTINUE;
        }

        if (lua_sched_post_zombie_thread(parent_coctx, ctx->cur_co_ctx) != 0) {
            return SCHED_ERROR;
        }

        lua_pushboolean(ctx->cur_co_ctx->co, 0);
        lua_insert(ctx->cur_co_ctx->co, 1);

        ctx->cur_co_ctx->co_status = LUA_SCHED_CO_ZOMBIE;
        ctx->cur_co_ctx = NULL;
        return SCHED_AGAIN;
    }

    if (ctx->cur_co_ctx->co_ref != LUA_NOREF) {
        lua_sched_del_co_ctx(ctx, ctx->cur_co_ctx);
        ctx->uthreads--;
    }

    if (ctx->uthreads == 0) {
        if (lua_sched_entry_thread_alive(ctx)) {
            ctx->cur_co_ctx = NULL;
            return SCHED_AGAIN;
        }
        return SCHED_DONE;
    }

    ctx->cur_co_ctx = NULL;
    return SCHED_AGAIN;
}

static int lua_sched_handle_user_coroutine_error(lua_sched_ctx* ctx, lua_sched_co_ctx* orig_coctx, int* nrets)
{
    lua_sched_co_ctx* next_coctx;
    lua_State* next_co;

    next_coctx = ctx->cur_co_ctx->parent_co_ctx;

    if (next_coctx == NULL) {
        lua_settop(ctx->L, 0);
        ctx->cur_co_ctx->co_status = LUA_SCHED_CO_DEAD;
        lua_sched_finalize_threads(ctx, ctx->L);
        DLOG("lua handler aborted: user coroutine has no parent\n");
        return SCHED_ERROR;
    }

    next_co = next_coctx->co;
    next_coctx->co_status = LUA_SCHED_CO_RUNNING;
    ctx->cur_co_ctx = next_coctx;

    if (orig_coctx->is_wrap) {
        /* coroutine.wrap propagates errors */
        next_coctx->propagate_error = 1;
        return SCHED_CONTINUE;
    }

    /* coroutine.resume returns false plus error message */
    lua_pushboolean(next_co, 0);
    lua_xmove(orig_coctx->co, next_co, 1);
    *nrets = 2;

    return SCHED_CONTINUE;
}

static int lua_sched_handle_coroutine_error(lua_sched_ctx* ctx, lua_State* L,
                                            lua_sched_co_ctx* orig_coctx,
                                            const char* err, const char* msg, int* nrets)
{
    if (ctx->cur_co_ctx != orig_coctx) {
        ctx->cur_co_ctx = orig_coctx;
    }

    ctx->cur_co_ctx->co_status = LUA_SCHED_CO_DEAD;

    if (orig_coctx->is_uthread || orig_coctx->is_wrap || lua_sched_is_entry_thread(ctx)) {
        lua_sched_thread_traceback(L, orig_coctx->co);

        if (lua_isstring(orig_coctx->co, -1)) {
            msg = lua_tostring(orig_coctx->co, -1);
        } else {
            msg = "unknown reason";
        }
    }

    if (ctx->cur_co_ctx->is_uthread) {
        return lua_sched_handle_user_thread_error(ctx, L, err, msg, nrets);
    }

    if (lua_sched_is_entry_thread(ctx)) {
        DLOG("lua entry thread aborted: %s: %s\n",
             err ? err : "error", msg ? msg : "no message");
        lua_settop(L, 0);
        lua_sched_finalize_threads(ctx, L);
        return SCHED_ERROR;
    }

    return lua_sched_handle_user_coroutine_error(ctx, orig_coctx, nrets);
}

int lua_sched_run_thread(lua_sched_ctx* ctx, int nrets)
{
    lua_sched_co_ctx* orig_coctx;
    lua_State* L = ctx->L;
    const char* err = NULL;
    const char* msg = NULL;
    int rv;
    int result;

    lua_atpanic(L, lua_sched_atpanic);

    if (ctx->cur_co_ctx == NULL) {
        DLOG("ERROR: ctx->cur_co_ctx is NULL\n");
        result = SCHED_ERROR;
        goto done;
    }

    /* Check if scheduler is paused */
    if (ctx->paused) {
        DLOG("Scheduler is paused, suspending coroutine\n");
        ctx->cur_co_ctx->co_status = LUA_SCHED_CO_SUSPENDED;
        result = SCHED_PAUSED;
        goto done;
    }

    if (ctx->cur_co_ctx->thread_spawn_yielded) {
        ctx->cur_co_ctx->thread_spawn_yielded = 0;
        nrets = 1;
    }

    if (ctx->cur_co_ctx->is_uthread && ctx->cur_co_ctx->nargs > 0) {
        nrets = ctx->cur_co_ctx->nargs;
        ctx->cur_co_ctx->nargs = 0;
    }

    while (1) {
        if (ctx->paused) {
            DLOG("Scheduler paused during execution\n");
            ctx->cur_co_ctx->co_status = LUA_SCHED_CO_SUSPENDED;
            result = SCHED_PAUSED;
            goto done;
        }
        orig_coctx = ctx->cur_co_ctx;

        if (orig_coctx->propagate_error) {
            orig_coctx->propagate_error = 0;
            result = lua_sched_handle_coroutine_error(ctx, L, orig_coctx, err, msg, &nrets);
            goto done;
        }

        /* Save resume context before calling lua_resume */
        orig_coctx->resume_from = L;
        orig_coctx->resume_nargs = nrets;

        /* Resume the coroutine */
        rv = lua_resume(orig_coctx->co, L, nrets, &nrets);

        switch (rv) {
            case LUA_YIELD:
                orig_coctx->co_top = lua_gettop(orig_coctx->co);

                if (ctx->exited) {
                    ctx->cur_co_ctx = NULL;
                    result = SCHED_DONE;
                    goto done;
                }

                result = lua_sched_handle_coroutine_operation(L, ctx, &nrets);
                if (result != SCHED_CONTINUE)
                    goto done;
                continue;
            case LUA_OK:
                result = lua_sched_handle_coroutine_success(ctx, L, &nrets);
                if (result == SCHED_CONTINUE) {
                    continue;
                } else if (result == SCHED_DONE) {
                    result = 0;
                    goto done;
                } else {
                    goto done;
                }
            case LUA_ERRRUN:
                err = "runtime error";
                DLOG("DEBUG: LUA_ERRRUN - getting error message\n");
                if (lua_isstring(orig_coctx->co, -1)) {
                    msg = lua_tostring(orig_coctx->co, -1);
                    DLOG("  Error: %s\n", msg);
                }
                break;
            case LUA_ERRSYNTAX:
                err = "syntax error";
                break;
            case LUA_ERRMEM:
                err = "memory allocation error";
                DLOG("[lua] %s\n", err);
                abort();
                break;
            case LUA_ERRERR:
                err = "error handler error";
                break;
            default:
                err = "unknown error";
                break;
        }

        /* Handle error */
        result = lua_sched_handle_coroutine_error(ctx, L, orig_coctx, err, msg, &nrets);
        if (result == SCHED_CONTINUE) {
            continue;
        } else {
            break;
        }
    }

done:
    return result;
}

int lua_sched_run_file(lua_sched_ctx* ctx, const char* filename)
{
    lua_State* L = ctx->L;
    lua_State* co;
    int co_ref;
    int rc;

    if (luaL_loadfile(L, filename) != 0) {
        DLOG("Failed to load file %s: %s\n",
             filename, lua_tostring(L, -1));
        lua_pop(L, 1);
        return SCHED_ERROR;
    }

    co = lua_sched_new_thread(L, &co_ref);
    if (co == NULL) {
        DLOG("Failed to create coroutine\n");
        return SCHED_ERROR;
    }

    lua_xmove(L, co, 1);

    ctx->cur_co_ctx = &ctx->entry_co_ctx;
    ctx->cur_co_ctx->co = co;
    ctx->cur_co_ctx->co_ref = co_ref;
    ctx->cur_co_ctx->co_top = 1;

    lua_sched_attach_co_ctx_to_L(L, co, ctx->cur_co_ctx);

    rc = lua_sched_run_thread(ctx, 0);

    if (rc == SCHED_AGAIN) {
        rc = lua_sched_run_posted_threads(L, ctx);
    } else if (rc == SCHED_PAUSED) {
        /* Keep SCHED_PAUSED status */
        return rc;
    }

    return rc;
}

int lua_sched_run_string(lua_sched_ctx* ctx, const char* script,
                         size_t script_len, const char* chunkname)
{
    lua_State* L = ctx->L;
    lua_State* co;
    int co_ref;
    int rc;

    if (luaL_loadbuffer(L, script, script_len, chunkname) != 0) {
        DLOG("Failed to load script: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return SCHED_ERROR;
    }

    co = lua_sched_new_thread(L, &co_ref);
    if (co == NULL) {
        DLOG("Failed to create coroutine\n");
        return SCHED_ERROR;
    }

    lua_xmove(L, co, 1);

    ctx->cur_co_ctx = &ctx->entry_co_ctx;
    ctx->cur_co_ctx->co = co;
    ctx->cur_co_ctx->co_ref = co_ref;
    ctx->cur_co_ctx->co_top = 1;

    lua_sched_attach_co_ctx_to_L(L, co, ctx->cur_co_ctx);

    rc = lua_sched_run_thread(ctx, 0);

    if (rc == SCHED_AGAIN) {
        rc = lua_sched_run_posted_threads(L, ctx);
    }

    return rc;
}

/* ============================================================================
 * Resume Context Access
 * ============================================================================ */

lua_State* lua_sched_get_resume_from(lua_sched_co_ctx* co_ctx)
{
    if (co_ctx == NULL) {
        return NULL;
    }
    return co_ctx->resume_from;
}

int lua_sched_get_resume_nargs(lua_sched_co_ctx* co_ctx)
{
    if (co_ctx == NULL) {
        return 0;
    }
    return co_ctx->resume_nargs;
}

lua_sched_co_ctx* lua_sched_find_co_ctx(lua_sched_ctx* ctx, lua_State* L)
{
    struct list* pos;

    if (ctx == NULL || L == NULL) {
        return NULL;
    }

    /* Check if it's the entry coroutine */
    if (ctx->entry_co_ctx.co == L) {
        return &ctx->entry_co_ctx;
    }

    /* Check current coroutine */
    if (ctx->cur_co_ctx && ctx->cur_co_ctx->co == L) {
        return ctx->cur_co_ctx;
    }

    /* Search through coroutine bucket */
    if (ctx->co_ctx_bucket) {
        list_for_each(pos, ctx->co_ctx_bucket) {
            lua_sched_co_ctx* co_ctx = list_entry(pos, lua_sched_co_ctx, link);
            if (co_ctx->co == L) {
                return co_ctx;
            }
        }
    }

    return NULL;
}