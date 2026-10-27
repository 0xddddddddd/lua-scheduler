#ifndef __LUA_SCHEDULER_STRUCT_H__
#define __LUA_SCHEDULER_STRUCT_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#	pragma once
#endif

#include <stdint.h>

#include "lua_import.h"
#include "lua_list.h"

 /* Coroutine status definitions */
typedef enum
{
    LUA_SCHED_CO_RUNNING = 0,  /* coroutine running */
    LUA_SCHED_CO_SUSPENDED = 1,  /* coroutine suspended */
    LUA_SCHED_CO_NORMAL = 2,  /* coroutine normal */
    LUA_SCHED_CO_DEAD = 3,  /* coroutine dead */
    LUA_SCHED_CO_ZOMBIE = 4,  /* coroutine zombie */
} lua_sched_co_status;

/* Coroutine operation types */
typedef enum
{
    LUA_SCHED_USER_CORO_NOP = 0,
    LUA_SCHED_USER_CORO_RESUME = 1,
    LUA_SCHED_USER_CORO_YIELD = 2,
    LUA_SCHED_USER_THREAD_RESUME = 3,
} lua_sched_user_coro_op;

// callback
typedef void (*co_ctx_cleanup)(void *data);

/* Forward declarations */
typedef struct lua_sched_posted_thread_s    lua_sched_posted_thread;
typedef struct lua_sched_event_s            lua_sched_event;
typedef struct lua_sched_co_ctx_s           lua_sched_co_ctx;
typedef struct lua_sched_ctx_s              lua_sched_ctx;

typedef void (lua_event_create)(lua_sched_ctx*, lua_sched_co_ctx*, uint64_t delay_ms);
typedef void (lua_event_destroy)(lua_sched_ctx*);
typedef void (lua_event_handler)(lua_sched_ctx*, lua_sched_co_ctx*);
typedef void (lua_event_complete)(lua_sched_ctx*, void* data);

/* Posted thread structure for thread queue */
struct lua_sched_posted_thread_s
{
    lua_sched_co_ctx*               co_ctx;
    unsigned                        high_priority : 1;  /* prioritized thread */
    lua_sched_co_ctx*               waited_by;
    lua_sched_posted_thread*        next;
};

struct lua_sched_event_s
{
    void*                       data;                       /* user state for custom data */
    lua_event_create*           create;
    lua_event_handler*          handler;
    lua_event_complete*         complete;
};

/* coroutine context structure */
struct lua_sched_co_ctx_s
{
    void*                       data;                       /* user state for custom data */

    lua_State*                  co;                         /* the coroutine state */
    lua_sched_co_ctx*           parent_co_ctx;              /* parent coroutine context */

    lua_sched_posted_thread*    zombie_child_threads;
    lua_sched_posted_thread**   next_zombie_child_thread;

    int                         co_ref;                     /* reference to anchor the coroutine */
    int                         co_top;                     /* stack top for sanity checks */
    int                         nargs;                      /* number of arguments for first resume */
    lua_sched_co_ctx*           waiting_for_thread;         /* thread this coroutine is waiting for */

    /* lua_resume context tracking */
    lua_State*                  resume_from;                /* lua_resume from parameter */
    int                         resume_nargs;               /* lua_resume nargs parameter */

    unsigned                    co_status : 3;              /* current coroutine status */
    unsigned                    co_entry : 1;
    unsigned                    is_uthread : 1;             /* is user thread */
    unsigned                    is_wrap : 1;                /* is coroutine.wrap */
    unsigned                    waited_by_parent : 1;       /* waited by parent */
    unsigned                    propagate_error : 1;        /* propagate error to parent */
    unsigned                    thread_spawn_yielded : 1;
    co_ctx_cleanup              cleanup;                    /* cleanup callback */
    unsigned                    index;
    struct list                 link;                       /* table */
};

/* main scheduler context */
struct lua_sched_ctx_s
{
    void*                       data;               /* user state for custom data */
    lua_State*                  L;                  /* main Lua state */

    lua_sched_co_ctx            entry_co_ctx;       /* entry coroutine context */
    lua_sched_co_ctx*           cur_co_ctx;         /* current coroutine context */
    struct list*                co_ctx_bucket;      /* coroutine contexts for user coroutines */

    lua_sched_posted_thread*    posted_threads;     /* posted thread queue */
    lua_sched_posted_thread*    paused_threads;     /* threads that tried to resume during pause */

    lua_sched_user_coro_op      co_op;              /* coroutine operation */

    unsigned                    uthreads;           /* number of user threads */
    unsigned                    evs;                /* number of user events */
    unsigned                    exited : 1;         /* scheduler exited flag */
    unsigned                    paused : 1;         /* scheduler paused flag */
    lua_sched_event             event;
};

#endif