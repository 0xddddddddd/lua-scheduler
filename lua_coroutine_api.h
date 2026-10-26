#ifndef __LUA_COROUTINE_API_H__
#define __LUA_COROUTINE_API_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#	pragma once
#endif

#include "lua_scheduler_struct.h"
#include "lua_scheduler_util.h"

/* Inject coroutine API into Lua state */
void lua_sched_inject_coroutine_api(lua_State* L);

#endif