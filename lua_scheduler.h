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

/* ============================================================================
 * Scheduler Lifecycle Management
 * ============================================================================ */

/**
 * lua_sched_init - Initialize the Lua scheduler
 * @L: Main Lua state
 * @event: Event handler callbacks (optional, can be NULL)
 * @data: User data pointer attached to scheduler context
 *
 * Creates and initializes a new scheduler context. Must be called before
 * any other scheduler operations.
 *
 * Returns: Scheduler context pointer on success, NULL on failure
 */
lua_sched_ctx* lua_sched_init(lua_State* L,
                              lua_sched_event* event,
                              void* data);

/**
 * lua_sched_cleanup - Cleanup and destroy the scheduler
 * @ctx: Scheduler context to cleanup
 *
 * Finalizes all threads, releases all resources, and frees the scheduler
 * context. Should be called when the scheduler is no longer needed.
 */
void lua_sched_cleanup(lua_sched_ctx* ctx);

/**
 * lua_sched_exit - Mark scheduler as exited
 * @ctx: Scheduler context
 *
 * Sets the exited flag to prevent further execution.
 */
void lua_sched_exit(lua_sched_ctx* ctx);

/**
 * lua_sched_stopped - Check if scheduler has stopped
 * @ctx: Scheduler context
 *
 * Returns: 1 if stopped/exited, 0 otherwise
 */
int lua_sched_stopped(lua_sched_ctx* ctx);

/**
 * lua_sched_force_stop - Force stop the scheduler immediately
 * @ctx: Scheduler context
 *
 * Forcefully stops all running coroutines, cleans up all queues and
 * releases all coroutine references. Use with caution.
 */
void lua_sched_force_stop(lua_sched_ctx* ctx);

/* ============================================================================
 * Script Execution
 * ============================================================================ */

/**
 * lua_sched_run_file - Load and execute a Lua script file
 * @ctx: Scheduler context
 * @filename: Path to the Lua script file
 *
 * Loads a Lua file, creates an entry coroutine, and runs it through the
 * scheduler. Handles posted threads automatically.
 *
 * Returns: 0 on success, negative on error, positive if yielded
 */
int lua_sched_run_file(lua_sched_ctx* ctx, const char* filename);

/**
 * lua_sched_run_string - Load and execute a Lua script string
 * @ctx: Scheduler context
 * @script: Lua script source code
 * @script_len: Length of the script string
 * @chunkname: Name for the script chunk (for error messages)
 *
 * Loads a Lua script from string, creates an entry coroutine, and runs it
 * through the scheduler. Handles posted threads automatically.
 *
 * Returns: 0 on success, negative on error, positive if yielded
 */
int lua_sched_run_string(lua_sched_ctx* ctx,
                         const char* script,
                         size_t script_len,
                         const char* chunkname);

/* ============================================================================
 * Coroutine Management
 * ============================================================================ */

/**
 * lua_sched_coroutine_create - Create a new coroutine
 * @L: Lua state (function should be at index 1)
 * @ctx: Scheduler context
 * @pcoctx: Output pointer to receive coroutine context (optional)
 * @co_ref: Output pointer to receive coroutine reference (optional)
 *
 * Creates a new coroutine from a Lua function. The function to wrap must
 * be at stack index 1. The coroutine is registered with the scheduler but
 * not automatically started.
 *
 * Returns: 1 on success, 0 on failure
 */
int lua_sched_coroutine_create(lua_State* L,
                               lua_sched_ctx* ctx,
                               lua_sched_co_ctx** pcoctx,
                               int* co_ref);

/**
 * lua_sched_create_co_ctx - Create a new coroutine context
 * @ctx: Scheduler context
 *
 * Allocates and initializes a new coroutine context structure.
 * The context is added to the scheduler's context bucket.
 *
 * Returns: Pointer to new context on success, NULL on failure
 */
lua_sched_co_ctx* lua_sched_create_co_ctx(lua_sched_ctx* ctx);

/**
 * lua_sched_get_co_ctx - Get coroutine context from Lua state
 * @co: Lua state of the coroutine
 * @ctx: Scheduler context
 *
 * Looks up the coroutine context associated with a given Lua state.
 *
 * Returns: Coroutine context pointer, or NULL if not found
 */
lua_sched_co_ctx* lua_sched_get_co_ctx(lua_State* co, lua_sched_ctx* ctx);

/**
 * lua_sched_del_co_ctx - Delete coroutine context and release reference
 * @ctx: Scheduler context
 * @co_ctx: Coroutine context to delete
 *
 * Releases the Lua registry reference for the coroutine and marks it as dead.
 * Does not free the context structure itself.
 */
void lua_sched_del_co_ctx(lua_sched_ctx* ctx, lua_sched_co_ctx* co_ctx);

/**
 * lua_sched_find_co_ctx - Find coroutine context by Lua state
 * @ctx: Scheduler context
 * @L: Lua state to search for
 *
 * Searches the scheduler's coroutine contexts (entry, current, and bucket)
 * to find the context associated with the given Lua state.
 *
 * Returns: Coroutine context pointer, or NULL if not found
 */
lua_sched_co_ctx* lua_sched_find_co_ctx(lua_sched_ctx* ctx, lua_State* L);

/* ============================================================================
 * Thread Management (Lua Threads)
 * ============================================================================ */

/**
 * lua_sched_new_thread - Create a new Lua thread (coroutine)
 * @L: Main Lua state
 * @ref: Output pointer to receive registry reference
 *
 * Creates a new Lua thread and stores it in the coroutines registry table
 * to prevent garbage collection.
 *
 * Returns: New Lua thread state, or NULL on failure
 */
lua_State* lua_sched_new_thread(lua_State* L, int* ref);

/**
 * lua_sched_del_thread - Delete a Lua thread and release reference
 * @L: Main Lua state
 * @co_ref: Registry reference to the thread
 *
 * Removes the thread from the coroutines registry table, allowing it to
 * be garbage collected.
 */
void lua_sched_del_thread(lua_State* L, int co_ref);

/* ============================================================================
 * Thread Queue Management
 * ============================================================================ */

/**
 * lua_sched_post_thread - Post a thread to the execution queue
 * @ctx: Scheduler context
 * @co_ctx: Coroutine context to post
 *
 * Adds a coroutine to the posted_threads queue for execution. Handles
 * priority and wait relationships automatically.
 *
 * Returns: 0 on success, -1 on memory allocation failure
 */
int lua_sched_post_thread(lua_sched_ctx* ctx, lua_sched_co_ctx* co_ctx);

/**
 * lua_sched_run_posted_threads - Execute all posted threads
 * @L: Main Lua state
 * @ctx: Scheduler context
 *
 * Processes the posted_threads queue, running each thread in turn.
 * Handles high-priority threads (waited-by parent) first.
 *
 * Returns: 0 on success, negative on error, SCHED_PAUSED if paused
 */
int lua_sched_run_posted_threads(lua_State* L, lua_sched_ctx* ctx);

/**
 * lua_sched_run_thread - Run a single thread
 * @ctx: Scheduler context
 * @nrets: Number of arguments to pass to the thread on resume
 *
 * Executes the current thread (ctx->cur_co_ctx) until it yields, finishes,
 * or encounters an error. Handles coroutine operations and state transitions.
 *
 * Returns: SCHED_CONTINUE, SCHED_AGAIN, SCHED_DONE, SCHED_PAUSED, or SCHED_ERROR
 */
int lua_sched_run_thread(lua_sched_ctx* ctx, int nrets);

/* ============================================================================
 * Queue Cleanup Functions
 * ============================================================================ */

/**
 * lua_sched_posted_threads_cleanup - Clean up posted threads queue
 * @ctx: Scheduler context
 *
 * Releases all coroutine references in posted_threads and frees the queue.
 */
void lua_sched_posted_threads_cleanup(lua_sched_ctx* ctx);

/**
 * lua_sched_paused_threads_cleanup - Clean up paused threads queue
 * @ctx: Scheduler context
 *
 * Releases all coroutine references in paused_threads and frees the queue.
 */
void lua_sched_paused_threads_cleanup(lua_sched_ctx* ctx);

/**
 * lua_sched_co_ctx_bucket_cleanup - Clean up all coroutine contexts
 * @ctx: Scheduler context
 *
 * Releases all coroutine references in the context bucket and cleans up
 * zombie child threads.
 */
void lua_sched_co_ctx_bucket_cleanup(lua_sched_ctx* ctx);

/* ============================================================================
 * Pause/Resume Control
 * ============================================================================ */

/**
 * lua_sched_pause - Pause the scheduler
 * @ctx: Scheduler context
 *
 * Sets the paused flag. Subsequent attempts to run threads will be suspended
 * until lua_sched_resume is called. Threads that try to resume during pause
 * are queued in paused_threads.
 */
void lua_sched_pause(lua_sched_ctx* ctx);

/**
 * lua_sched_resume - Resume the scheduler
 * @ctx: Scheduler context
 *
 * Clears the paused flag and moves all paused_threads back to the
 * posted_threads queue for execution.
 */
void lua_sched_resume(lua_sched_ctx* ctx);

/**
 * lua_sched_is_paused - Check if scheduler is paused
 * @ctx: Scheduler context
 *
 * Returns: 1 if paused, 0 if running
 */
int lua_sched_is_paused(lua_sched_ctx* ctx);

/**
 * lua_sched_is_all_done - Check if all scripts and threads have completed
 * @ctx: Scheduler context
 *
 * Checks if the entire script execution has naturally completed by verifying:
 * - Entry coroutine is dead (main script finished)
 * - No user threads are running (uthreads == 0)
 * - No threads in posted_threads queue
 * - No threads in paused_threads queue
 *
 * This function is useful for determining when to cleanup or exit after
 * running a script with lua_sched_run_file or lua_sched_run_string.
 *
 * Returns: 1 if all execution completed, 0 if still running
 */
int lua_sched_is_all_done(lua_sched_ctx* ctx);

/* ============================================================================
 * Resume Context Access (for lua_resume parameter tracking)
 * ============================================================================ */

/**
 * lua_sched_get_resume_from - Get the 'from' parameter of lua_resume
 * @co_ctx: Coroutine context
 *
 * Returns the lua_State* that was passed as the 'from' parameter to
 * lua_resume when this coroutine was last resumed.
 *
 * Returns: lua_State* or NULL
 */
lua_State* lua_sched_get_resume_from(lua_sched_co_ctx* co_ctx);

/**
 * lua_sched_get_resume_nargs - Get the 'nargs' parameter of lua_resume
 * @co_ctx: Coroutine context
 *
 * Returns the number of arguments that were passed to lua_resume when
 * this coroutine was last resumed.
 *
 * Returns: Number of arguments (0 or positive)
 */
int lua_sched_get_resume_nargs(lua_sched_co_ctx* co_ctx);

#ifdef __cplusplus
}
#endif

#endif // __LUA_SCHEDULER_H__