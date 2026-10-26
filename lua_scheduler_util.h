#ifndef __LUA_SCHEDULER_UTIL_H__
#define __LUA_SCHEDULER_UTIL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#	pragma once
#endif

#include <stdio.h>
#include <stdint.h>

#include "lua_scheduler_struct.h"

/* Helper macros */
#define lua_sched_lightudata_mask(ludata) (&lua_sched_##ludata)

#define lua_sched_is_entry_thread(ctx) \
    ((ctx)->cur_co_ctx == &(ctx)->entry_co_ctx)

#define lua_sched_is_thread(ctx) \
    ((ctx)->cur_co_ctx && (ctx)->cur_co_ctx->is_uthread)

#define lua_sched_entry_thread_alive(ctx) \
    ((ctx)->entry_co_ctx.co_status != LUA_SCHED_CO_DEAD && (ctx)->entry_co_ctx.co_ref != LUA_NOREF)

#define lua_is_entry_thread(ctx)                                    \
    ((ctx)->cur_co_ctx == &(ctx)->entry_co_ctx)

#define lua_sched_coroutine_alive(coctx)                                  \
    ((coctx)->co_status != LUA_SCHED_CO_DEAD                              \
     && (coctx)->co_status != LUA_SCHED_CO_ZOMBIE)

/* Return codes for internal functions */
#define SCHED_CONTINUE 0
#define SCHED_AGAIN 1
#define SCHED_DONE 2
#define SCHED_ERROR -1

#ifndef DLOG
#   define DLOG(...) printf(__VA_ARGS__)
#else
#   define DLOG(...)
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Registry keys (as light userdata pointers) */
extern char lua_sched_coroutines_key;
extern char lua_sched_co_ctx_key;
extern char lua_sched_ctx_registry_key;

void lua_sched_attach_co_ctx_to_L(lua_State* L,
                                  lua_State* co,
                                  lua_sched_co_ctx* coctx);

/* Get scheduler context from Lua registry */
lua_sched_ctx* lua_registry_get(lua_State* L);
void lua_registry_set(lua_State* L, lua_sched_ctx* ctx);

#ifdef __cplusplus
}
#endif
#endif