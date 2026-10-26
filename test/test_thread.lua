-- Test script for user thread API
-- Demonstrates thread.spawn, thread.wait, thread.kill

print("=== Testing User Thread API ===")
print()

-- Test 1: Simple thread spawn and wait
print("Test 1: Spawn a single thread")
local thread1 = thread.spawn(function()
    print("  Thread 1: Starting")
    print("  Thread 1: Doing work...")
    for i = 1, 3 do
        print("  Thread 1: Step " .. i)
        coroutine.yield()
    end
    print("  Thread 1: Finished")
    return "result-1"
end)

print("Main: Thread spawned, waiting...")
local result = thread.wait(thread1)
print("Main: Thread finished with result: " .. tostring(result))

print()

-- Test 2: Multiple threads
print("Test 2: Spawn multiple threads")
local thread2 = thread.spawn(function()
    print("  Thread 2: Working...")
    coroutine.yield()
    print("  Thread 2: Done")
    return "result-2"
end)

local thread3 = thread.spawn(function()
    print("  Thread 3: Working...")
    coroutine.yield()
    print("  Thread 3: Done")
    return "result-3"
end)

print("Main: Waiting for thread 2")
local result2 = thread.wait(thread2)
print("Main: Thread 2 returned: " .. result2)

print("Main: Waiting for thread 3")
local result3 = thread.wait(thread3)
print("Main: Thread 3 returned: " .. result3)

print()

-- Test 3: Thread with arguments
print("Test 3: Thread with arguments")
local thread4 = thread.spawn(function(name, count)
    print("  Thread: Hello from " .. name)
    for i = 1, count do
        print("  Thread: Iteration " .. i)
        coroutine.yield()
    end
    return name .. " completed " .. count .. " iterations"
end, "Worker-4", 2)

local result4 = thread.wait(thread4)
print("Main: Result: " .. result4)

print()

-- Test 4: Thread that yields values
print("Test 4: Thread communication")
local producer = thread.spawn(function()
    print("  Producer: Generating values")
    for i = 1, 3 do
        print("  Producer: Generated value " .. i)
        coroutine.yield()
    end
    return "production-complete"
end)

print("Main: Letting producer run...")
coroutine.yield()  -- Let producer run
coroutine.yield()  -- Let producer run more
coroutine.yield()  -- Let producer finish

local result = thread.wait(producer)
print("Main: Producer result: " .. result)

print()

-- Test 5: Parent-child thread relationship
print("Test 5: Parent spawns child threads")

local function parent_worker()
    print("  Parent: Starting")

    local child1 = thread.spawn(function()
        print("    Child 1: Working")
        coroutine.yield()
        print("    Child 1: Done")
        return 100
    end)

    local child2 = thread.spawn(function()
        print("    Child 2: Working")
        coroutine.yield()
        print("    Child 2: Done")
        return 200
    end)

    print("  Parent: Waiting for children")
    local r1 = thread.wait(child1)
    local r2 = thread.wait(child2)

    print("  Parent: Children returned " .. r1 .. " and " .. r2)
    return r1 + r2
end

local parent = thread.spawn(parent_worker)
local final_result = thread.wait(parent)
print("Main: Parent returned: " .. final_result)

print()
print("=== All Thread Tests Completed Successfully ===")
