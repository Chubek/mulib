-- Execute with: ./parallel-sh --lua examples/parallel.lua
channel("messages")
send("messages", "hello from producer")
print(recv("messages"))
for i = 1, 4 do
  task("echo {}", tostring(i))
end
-- Each Lua task executes in a fresh, separate Mune state in a child process.
lua_task("print('hello from a Mune child')")
assert(run())
