-- Test script for basic coroutine scheduling
-- Demonstrates coroutine.create, coroutine.resume, coroutine.yield

print("=== Testing Basic Coroutine Scheduling ===")
print()

-- Test 1: Simple coroutine
print("Test 1: Simple coroutine yield and resume")
local co1 = coroutine.create(function()
    print("  Coroutine: Starting")
    coroutine.yield()
    print("  Coroutine: Resumed")
    coroutine.yield()
    print("  Coroutine: Final resume")
    return "done"
end)

print("  Main: Created coroutine")
print("  Main: Status = " .. coroutine.status(co1))

local ok, result = coroutine.resume(co1)
print("  Main: First resume, ok=" .. tostring(ok))

local ok, result = coroutine.resume(co1)
print("  Main: Second resume, ok=" .. tostring(ok))

local ok, result = coroutine.resume(co1)
print("  Main: Third resume, ok=" .. tostring(ok) .. ", result=" .. tostring(result))
print("  Main: Status = " .. coroutine.status(co1))

print()

-- Test 2: Coroutine with values
print("Test 2: Coroutine passing values")
local co2 = coroutine.create(function(a, b)
    print("  Coroutine: Received a=" .. a .. ", b=" .. b)
    local x, y = coroutine.yield(a + b)
    print("  Coroutine: Received x=" .. x .. ", y=" .. y)
    return x * y
end)

local ok, sum = coroutine.resume(co2, 10, 20)
print("  Main: Got sum = " .. sum)

local ok, product = coroutine.resume(co2, 5, 6)
print("  Main: Got product = " .. product)

print()

-- Test 3: Nested coroutines
print("Test 3: Nested coroutines")
local co_inner = coroutine.create(function()
    print("    Inner: Starting")
    coroutine.yield("inner-yield")
    print("    Inner: Finished")
    return "inner-done"
end)

local co_outer = coroutine.create(function()
    print("  Outer: Starting")
    coroutine.yield("outer-yield-1")

    print("  Outer: Resuming inner coroutine")
    local ok, val = coroutine.resume(co_inner)
    print("  Outer: Inner yielded: " .. val)

    coroutine.yield("outer-yield-2")

    local ok, val = coroutine.resume(co_inner)
    print("  Outer: Inner returned: " .. val)

    return "outer-done"
end)

print("Main: Resume outer #1")
local ok, val = coroutine.resume(co_outer)
print("Main: Got: " .. val)

print("Main: Resume outer #2")
local ok, val = coroutine.resume(co_outer)
print("Main: Got: " .. val)

print("Main: Resume outer #3")
local ok, val = coroutine.resume(co_outer)
print("Main: Got: " .. val)

print()

-- Test 4: coroutine.wrap
print("Test 4: coroutine.wrap")
local count = 0
local wrapped = coroutine.wrap(function()
    while count < 3 do
        count = count + 1
        print("  Wrapped: count = " .. count)
        coroutine.yield(count)
    end
    return "wrapped-done"
end)

print("Main: Call wrapped #1, result = " .. wrapped())
print("Main: Call wrapped #2, result = " .. wrapped())
print("Main: Call wrapped #3, result = " .. wrapped())
print("Main: Call wrapped #4, result = " .. wrapped())

print()
print("=== All Tests Completed Successfully ===")
