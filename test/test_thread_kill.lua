-- Test thread.spawn, thread.wait, and thread.kill
print("=== Testing Thread Spawn, Wait, and Kill ===")
print()

-- Test 1: Basic spawn and wait
print("Test 1: Basic spawn and wait")
local t1 = thread.spawn(function()
    print("  [T1] Thread started")
    coroutine.yield()
    print("  [T1] Thread continuing")
    return "t1-result"
end)
print("Main: T1 spawned")

local result = thread.wait(t1)
print("Main: T1 result:", result)
print()

-- Test 2: Kill a thread before waiting for it
print("Test 2: Kill a thread before waiting")
local t2 = thread.spawn(function()
    print("  [T2] Thread started")
    coroutine.yield()
    print("  [T2] This should NOT print (will be killed)")
    coroutine.yield()
    print("  [T2] Still should not print")
    return "t2-result"
end)
print("Main: T2 spawned")

-- Wait for T2 to start
local dummy2 = thread.wait(t2)  -- This will let T2 run once and yield
print("Main: T2 yielded, now killing it")

-- Kill it before it finishes
local ok = thread.kill(t2)
print("Main: T2 killed, ok =", ok)

-- Try to wait for killed thread
local result2, err = thread.wait(t2)
print("Main: T2 wait result:", result2, err)
print()

-- Test 3: Kill immediately after spawn (before it runs)
print("Test 3: Kill immediately after spawn")
local t3 = thread.spawn(function()
    print("  [T3] This should NOT print (killed early)")
    return "t3-result"
end)
print("Main: T3 spawned")

-- Kill immediately before it has a chance to run
local ok3 = thread.kill(t3)
print("Main: T3 killed immediately, ok =", ok3)

-- Try to wait
local result3, err3 = thread.wait(t3)
print("Main: T3 wait result:", result3, err3)
print()

-- Test 4: Multiple threads with selective kill
print("Test 4: Multiple threads with selective kill")
local t4a = thread.spawn(function()
    print("  [T4A] Started")
    coroutine.yield()
    print("  [T4A] Continuing")
    return "t4a-done"
end)

local t4b = thread.spawn(function()
    print("  [T4B] Started")
    coroutine.yield()
    print("  [T4B] This should NOT print (will be killed)")
    return "t4b-done"
end)

local t4c = thread.spawn(function()
    print("  [T4C] Started")
    coroutine.yield()
    print("  [T4C] Continuing")
    return "t4c-done"
end)

print("Main: T4A, T4B, T4C spawned")

-- Kill T4B immediately
thread.kill(t4b)
print("Main: T4B killed")

-- Wait for T4A (should work)
local r4a = thread.wait(t4a)
print("Main: T4A result:", r4a)

-- Wait for T4C (should work)
local r4c = thread.wait(t4c)
print("Main: T4C result:", r4c)

-- Try to wait for killed T4B
local r4b, e4b = thread.wait(t4b)
print("Main: T4B result:", r4b, e4b)
print()

-- Test 5: Kill already finished thread
print("Test 5: Kill already finished thread")
local t5 = thread.spawn(function()
    print("  [T5] Quick finish")
    return "t5-done"
end)

local r5 = thread.wait(t5)
print("Main: T5 result:", r5)

-- Try to kill finished thread
local ok5, err5 = thread.kill(t5)
print("Main: Kill finished thread:", ok5, err5)
print()

-- Test 6: Double kill
print("Test 6: Double kill")
local t6 = thread.spawn(function()
    print("  [T6] Running")
    coroutine.yield()
    print("  [T6] Should not reach here")
end)

-- Kill before it runs
local ok6a = thread.kill(t6)
print("Main: First kill:", ok6a)

-- Try to kill again
local ok6b, err6b = thread.kill(t6)
print("Main: Second kill:", ok6b, err6b)
print()

print("=== All Tests Complete ===")
