-- 超级简化的调试测试
print("=== Debug Test ===")

print("Step 1: Creating coroutine")
local co = coroutine.create(function()
    print("  [CO] Inside coroutine!")
    return 42
end)

print("Step 2: Checking status")
print("  Status: " .. coroutine.status(co))

print("Step 3: Calling resume")
local results = {coroutine.resume(co)}
print("  Resume returned " .. #results .. " values")
for i, v in ipairs(results) do
    print("    [" .. i .. "] = " .. tostring(v))
end

print("Step 4: Checking status again")
print("  Status: " .. coroutine.status(co))

print("=== End ===")
