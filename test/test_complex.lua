-- Complex test combining coroutines and threads
print("=== Complex Coroutine and Thread Test ===")
print()

-- Test 1: Thread spawning threads (nested threading)
print("Test 1: Nested threading - thread spawns child threads")
local parent_thread = thread.spawn(function()
    print("  [Parent] Starting")

    local child1 = thread.spawn(function()
        print("    [Child1] Working")
        coroutine.yield()
        print("    [Child1] Done")
        return "child1-result"
    end)

    local child2 = thread.spawn(function()
        print("    [Child2] Working")
        coroutine.yield()
        print("    [Child2] Done")
        return "child2-result"
    end)

    print("  [Parent] Waiting for children")
    local r1 = thread.wait(child1)
    local r2 = thread.wait(child2)

    print("  [Parent] Children finished:", r1, r2)
    return "parent-done"
end)

local result = thread.wait(parent_thread)
print("Main: Parent thread result:", result)
print()

-- Test 2: Coroutines within threads
print("Test 2: Coroutines within threads")
local thread_with_coro = thread.spawn(function()
    print("  [Thread] Creating coroutine")

    local co = coroutine.create(function(name)
        print("    [Coro] Hello from", name)
        coroutine.yield("first-yield")
        print("    [Coro] Resumed")
        return "coro-done"
    end)

    local ok, val = coroutine.resume(co, "thread-coro")
    print("  [Thread] Coro yielded:", ok, val)

    local ok2, val2 = coroutine.resume(co)
    print("  [Thread] Coro finished:", ok2, val2)

    return "thread-coro-complete"
end)

local r2 = thread.wait(thread_with_coro)
print("Main:", r2)
print()

-- Test 3: Producer-Consumer with threads
print("Test 3: Producer-Consumer pattern with threads")
local shared_data = {}

local producer = thread.spawn(function()
    print("  [Producer] Starting production")
    for i = 1, 5 do
        print("  [Producer] Producing item", i)
        table.insert(shared_data, "item-" .. i)
        coroutine.yield()
    end
    print("  [Producer] Production complete")
    return "produced-5-items"
end)

local consumer = thread.spawn(function()
    print("  [Consumer] Starting consumption")
    local consumed = 0
    while consumed < 5 do
        if #shared_data > consumed then
            consumed = consumed + 1
            local item = shared_data[consumed]
            print("  [Consumer] Consumed", item)
        end
        coroutine.yield()
    end
    print("  [Consumer] Consumption complete")
    return "consumed-5-items"
end)

-- Let them run
print("Main: Waiting for producer")
local prod_result = thread.wait(producer)
print("Main: Producer finished:", prod_result)

print("Main: Waiting for consumer")
local cons_result = thread.wait(consumer)
print("Main: Consumer finished:", cons_result)
print()

-- Test 4: Thread pipeline with data transformation
print("Test 4: Thread pipeline")
local stage1_result = nil
local stage2_result = nil

local stage1 = thread.spawn(function()
    print("  [Stage1] Processing input")
    coroutine.yield()
    local data = "raw-data"
    print("  [Stage1] Transforming:", data)
    return data:upper()
end)

local stage2 = thread.spawn(function()
    print("  [Stage2] Waiting for stage1")
    -- Stage1 needs to finish first
    coroutine.yield()
    coroutine.yield()
    local input = stage1_result
    print("  [Stage2] Received:", input)
    return input .. "-PROCESSED"
end)

stage1_result = thread.wait(stage1)
print("Main: Stage1 output:", stage1_result)

stage2_result = thread.wait(stage2)
print("Main: Stage2 output:", stage2_result)
print()

-- Test 5: Thread pool simulation
print("Test 5: Thread pool with task queue")
local tasks = {"task-a", "task-b", "task-c", "task-d"}
local results = {}

local function worker(id, task)
    return thread.spawn(function()
        print("  [Worker" .. id .. "] Processing", task)
        coroutine.yield()
        local result = task:upper() .. "-DONE"
        print("  [Worker" .. id .. "] Completed", result)
        return result
    end)
end

-- Spawn workers
local workers = {}
for i, task in ipairs(tasks) do
    workers[i] = worker(i, task)
end

-- Collect results
print("Main: Collecting results from", #workers, "workers")
for i, w in ipairs(workers) do
    results[i] = thread.wait(w)
    print("Main: Worker", i, "returned:", results[i])
end
print()

-- Test 6: Error handling in nested threads
print("Test 6: Error handling in nested threads")
local error_thread = thread.spawn(function()
    print("  [ErrorThread] Starting")

    local bad_child = thread.spawn(function()
        print("    [BadChild] About to error")
        error("intentional error")
    end)

    local ok, err = thread.wait(bad_child)
    print("  [ErrorThread] Child error caught:", ok, err)

    return "error-handled"
end)

local r6 = thread.wait(error_thread)
print("Main: Error thread result:", r6)
print()

-- Test 7: Selective thread execution with kill
print("Test 7: Dynamic thread management with kill")
local long_runner = thread.spawn(function()
    print("  [LongRunner] Starting long task")
    for i = 1, 10 do
        print("  [LongRunner] Step", i)
        coroutine.yield()
    end
    return "completed-all-10-steps"
end)

local quick_task = thread.spawn(function()
    print("  [QuickTask] Quick work")
    return "quick-done"
end)

-- Wait for quick task
local quick_result = thread.wait(quick_task)
print("Main: Quick task done:", quick_result)

-- Kill long runner after a few steps
print("Main: Killing long runner")
thread.kill(long_runner)

local long_result, long_err = thread.wait(long_runner)
print("Main: Long runner result:", long_result, long_err)
print()

-- Test 8: Coroutine wrap with threads
print("Test 8: Using coroutine.wrap with threads")
local wrapper_thread = thread.spawn(function()
    print("  [Thread] Creating wrapped coroutine")

    local wrapped = coroutine.wrap(function()
        for i = 1, 3 do
            print("    [Wrapped] Iteration", i)
            coroutine.yield(i * 10)
        end
        return "wrap-complete"
    end)

    local v1 = wrapped()
    print("  [Thread] Got:", v1)

    local v2 = wrapped()
    print("  [Thread] Got:", v2)

    local v3 = wrapped()
    print("  [Thread] Got:", v3)

    local v4 = wrapped()
    print("  [Thread] Got:", v4)

    return "wrapper-finished"
end)

local r8 = thread.wait(wrapper_thread)
print("Main:", r8)
print()

-- Test 9: Multiple coroutine.yield in main with active threads
print("Test 9: Main yields while threads are running")
local bg_thread = thread.spawn(function()
    print("  [BG] Background work starting")
    for i = 1, 3 do
        coroutine.yield()
        print("  [BG] Background step", i)
    end
    return "bg-done"
end)

print("Main: Spawned background thread, now yielding")
coroutine.yield()
print("Main: Resumed from first yield")
coroutine.yield()
print("Main: Resumed from second yield")
coroutine.yield()
print("Main: Resumed from third yield, collecting background result")

local r9 = thread.wait(bg_thread)
print("Main: Background result:", r9)
print()

print("=== All Complex Tests Complete ===")
