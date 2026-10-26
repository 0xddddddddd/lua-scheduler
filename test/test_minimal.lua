-- 最简单的测试，用于调试
print("=== Minimal Test ===")

-- Test 1: 最基本的协程
print("\nTest 1: Basic coroutine")
local co = coroutine.create(function()
    print("  In coroutine")
    return "done"
end)

print("Created coroutine, status:", coroutine.status(co))
local ok, result = coroutine.resume(co)
print("Resumed, ok:", ok, "result:", result)
print("Status:", coroutine.status(co))

print("\n=== Test Complete ===")
