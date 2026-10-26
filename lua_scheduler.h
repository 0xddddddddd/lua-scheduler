#ifndef __LUA_SCHEDULER_H__
#define __LUA_SCHEDULER_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#	pragma once
#endif

#include <stdint.h>

#include "lua_scheduler_util.h"
#include "lua_scheduler_struct.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Public API */
/* Initialize the Lua scheduler */
lua_sched_ctx* lua_sched_init(lua_State* L);

/* Cleanup the scheduler */
void lua_sched_cleanup(lua_sched_ctx* ctx);

/* Load and run a Lua script file */
int lua_sched_run_file(lua_sched_ctx* ctx, const char* filename);

/* Load and run a Lua script string */
int lua_sched_run_string(lua_sched_ctx* ctx, const char* script,
                         size_t script_len, const char* chunkname);

/* Create a new coroutine */
int lua_sched_coroutine_create(lua_State* L, lua_sched_ctx* ctx,
                               lua_sched_co_ctx** pcoctx, int* co_ref);

/* Run the scheduler thread */
int lua_sched_run_thread(lua_sched_ctx* ctx, int nrets);

/* Post a thread to the scheduler queue */
int lua_sched_post_thread(lua_sched_ctx* ctx, lua_sched_co_ctx* co_ctx);

/* Get coroutine context from Lua state */
lua_sched_co_ctx* lua_sched_get_co_ctx(lua_State* co, lua_sched_ctx* ctx);

/* Create a new coroutine context */
lua_sched_co_ctx* lua_sched_create_co_ctx(lua_sched_ctx* ctx);

/* Delete a coroutine context */
void lua_sched_del_co_ctx(lua_sched_ctx* ctx, lua_sched_co_ctx* co_ctx);
void lua_sched_del_co_ctx_free(lua_sched_ctx* ctx, lua_sched_co_ctx* co_ctx);
/* Utility: Create a new Lua thread */
lua_State* lua_sched_new_thread(lua_State* L, int* ref);

/* Utility: Delete a Lua thread */
void lua_sched_del_thread(lua_State* L, int co_ref);

#ifdef __cplusplus
}
#endif

#endif // __LUA_SCHEDULER_H__
