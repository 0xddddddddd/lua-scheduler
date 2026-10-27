#ifndef __LUA_SLEEP_API_H__
#define __LUA_SLEEP_API_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#	pragma once
#endif

#include "lua_scheduler_struct.h"
#include "lua_scheduler_util.h"

/* Inject thread API into Lua state */
void lua_sched_inject_sleep_api(lua_State *L);
void lua_sched_sleep_resume(lua_sched_ctx* ctx, lua_sched_co_ctx* coctx);

#endif