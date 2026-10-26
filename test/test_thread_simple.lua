-- Simplified thread test
print("=== Simple Thread Test ===")

print("Step 1: Spawning thread")
local t = thread.spawn(function()
    print("  [Thread] Started")
    return "done"
end)

print("Step 2: Waiting for thread")
local result = thread.wait(t)
print("Step 3: Got result: " .. tostring(result))

print("=== Test Complete ===")
