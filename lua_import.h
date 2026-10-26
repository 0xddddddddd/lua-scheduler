#ifndef __LUA_IMPORT_H__
#define __LUA_IMPORT_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#	pragma once
#endif

#if defined(__cplusplus)
#   include <lua.hpp>
#else
#   include <lua.h>
#   include <lualib.h>
#   include <lauxlib.h>
#endif

#endif