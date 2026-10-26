-- Test script for nested coroutines and complex scenarios
-- Demonstrates complex coroutine scheduling patterns

print("=== Testing Nested Coroutines and Complex Scenarios ===")
print()

-- Test 1: Producer-Consumer pattern
print("Test 1: Producer-Consumer Pattern")

local function producer(consumer)
    print("  Producer: Starting production")
    for i = 1, 5 do
        print("  Producer: Producing item " .. i)
        local ok, msg = coroutine.resume(consumer, i)
        if not ok then
            print("  Producer: Consumer error: " .. msg)
            break
        end
    end
    print("  Producer: Production complete")
end

local function consumer_func()
    print("  Consumer: Ready to consume")
    while true do
        local item = coroutine.yield()
        if item == nil then break end
        print("  Consumer: Consumed item " .. item)
    end
end

local consumer = coroutine.create(consumer_func)
coroutine.resume(consumer)  -- Start consumer
producer(consumer)

print()

-- Test 2: Coroutine chain
print("Test 2: Coroutine Chain")

local co_c = coroutine.create(function(val)
    print("    C: Received " .. val)
    return val * 3
end)

local co_b = coroutine.create(function(val)
    print("   B: Received " .. val)
    local ok, result = coroutine.resume(co_c, val * 2)
    print("   B: C returned " .. result)
    return result + 10
end)

local co_a = coroutine.create(function(val)
    print("  A: Received " .. val)
    local ok, result = coroutine.resume(co_b, val + 5)
    print("  A: B returned " .. result)
    return result + 100
end)

local ok, final = coroutine.resume(co_a, 1)
print("Main: Final result = " .. final)

print()

-- Test 3: Cooperative multitasking
print("Test 3: Cooperative Multitasking")

local tasks = {}
local task_count = 0

local function create_task(name, steps)
    task_count = task_count + 1
    local task = coroutine.create(function()
        print("  Task " .. name .. ": Starting")
        for i = 1, steps do
            print("  Task " .. name .. ": Step " .. i .. "/" .. steps)
            coroutine.yield()
        end
        print("  Task " .. name .. ": Completed")
        return name .. "-done"
    end)
    table.insert(tasks, {name = name, coro = task, active = true})
end

-- Create multiple tasks
create_task("A", 3)
create_task("B", 4)
create_task("C", 2)

-- Run tasks cooperatively
print("Main: Running tasks cooperatively")
local active_count = #tasks
while active_count > 0 do
    for i, task in ipairs(tasks) do
        if task.active then
            local ok, result = coroutine.resume(task.coro)
            if coroutine.status(task.coro) == "dead" then
                print("Main: Task " .. task.name .. " finished: " .. tostring(result))
                task.active = false
                active_count = active_count - 1
            end
        end
    end
end

print("Main: All tasks completed")

print()

-- Test 4: Recursive coroutines
print("Test 4: Recursive Coroutine Pattern")

local function recursive_worker(depth, max_depth)
    print(string.rep("  ", depth) .. "Level " .. depth .. ": Start")

    if depth < max_depth then
        local child = coroutine.create(function()
            return recursive_worker(depth + 1, max_depth)
        end)

        local ok, result = coroutine.resume(child)
        print(string.rep("  ", depth) .. "Level " .. depth .. ": Child returned " .. result)

        coroutine.yield()
    end

    print(string.rep("  ", depth) .. "Level " .. depth .. ": End")
    return depth * 10
end

local root = coroutine.create(function()
    return recursive_worker(1, 4)
end)

local ok, result = coroutine.resume(root)
print("Main: Initial result = " .. tostring(result))

while coroutine.status(root) ~= "dead" do
    ok, result = coroutine.resume(root)
    if ok and result then
        print("Main: Intermediate result = " .. tostring(result))
    end
end

print()

-- Test 5: Error handling in coroutines
print("Test 5: Error Handling")

local safe_co = coroutine.create(function()
    print("  Safe coroutine: Starting")
    coroutine.yield("checkpoint-1")
    print("  Safe coroutine: Continuing")
    coroutine.yield("checkpoint-2")
    print("  Safe coroutine: Finishing")
    return "success"
end)

-- Resume safely
local function safe_resume(co, ...)
    local results = {coroutine.resume(co, ...)}
    local ok = results[1]
    if ok then
        print("  Resume OK: " .. tostring(results[2]))
    else
        print("  Resume Error: " .. tostring(results[2]))
    end
    return table.unpack(results)
end

safe_resume(safe_co)
safe_resume(safe_co)
safe_resume(safe_co)

print()

-- Test 6: Coroutine state machine
print("Test 6: State Machine with Coroutines")

local function state_machine()
    local state = "INIT"

    print("  State Machine: Entering INIT state")
    state = "READY"
    coroutine.yield(state)

    print("  State Machine: Entering READY state")
    state = "RUNNING"
    coroutine.yield(state)

    print("  State Machine: Entering RUNNING state")
    state = "PAUSED"
    coroutine.yield(state)

    print("  State Machine: Entering PAUSED state")
    state = "FINISHED"

    return state
end

local sm = coroutine.create(state_machine)

while coroutine.status(sm) ~= "dead" do
    local ok, state = coroutine.resume(sm)
    if state then
        print("Main: State = " .. state)
    end
end

print()
print("=== All Complex Tests Completed Successfully ===")
