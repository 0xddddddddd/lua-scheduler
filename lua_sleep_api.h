#ifndef __LUA_SLEEP_API_H__
#define __LUA_SLEEP_API_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#	pragma once
#endif

#include "lua_scheduler_struct.h"
#include "lua_scheduler_util.h"

/* Inject thread API into Lua state */
void ngx_http_lua_inject_sleep_api(lua_State *L);

#endif