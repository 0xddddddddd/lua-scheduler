#include <stdio.h>
#include <stdlib.h>

#include "lua_scheduler.h"

int main(int argc, char *argv[])
{
    lua_State *L;
    lua_sched_ctx *ctx;
    int rc;
    const char *filename;

    /* Check arguments */
    /*if (argc < 2) {
        printf("Usage: %s <lua_script.lua>\n", argv[0]);
        printf("\nExample Lua scripts you can test:\n");
        printf("  - test_coroutine.lua  : Test basic coroutine scheduling\n");
        printf("  - test_thread.lua     : Test user thread spawning and waiting\n");
        printf("  - test_nested.lua     : Test nested coroutines\n");
        return 1;
    }*/

    filename = "./test_nested.lua";

    L = luaL_newstate();
    if (L == NULL) {
        DLOG("Error: Cannot create Lua state\n");
        return 1;
    }

    luaL_openlibs(L);

    ctx = lua_sched_init(L);
    if (ctx == NULL) {
        DLOG("Error: Cannot initialize scheduler\n");
        lua_close(L);
        return 1;
    }

    printf("Loading and running script: %s\n\n", filename);

    rc = lua_sched_run_file(ctx, filename);

    if (rc == 0) {
        printf("\nScript executed successfully!\n");
    } else if (rc < 0) {
        printf("\nScript execution failed with error code: %d\n", rc);
    } else {
        printf("\nScript yielded with code: %d\n", rc);
    }

    printf("Cleaning up scheduler...\n");
    lua_sched_cleanup(ctx);
    lua_close(L);

    printf("Done!\n");
    return (rc == 0) ? 0 : 1;
}
