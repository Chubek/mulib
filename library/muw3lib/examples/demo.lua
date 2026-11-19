-- From the repository root; offline unless an explicit URL is supplied:
--   build/muw3 --weblet library/muw3lib/examples/demo.lua [URL]
local parent = arg[0]:match('^(.*)/muw3lib/examples/[^/]+$') or 'library'
package.path = parent .. '/?.lua;' .. parent .. '/?/init.lua;' .. package.path
local api = require('muw3lib')
if not arg[1] then
  local events = api.sse.parse('event: demo\ndata: hello from Mune\n\n')
  print('muw3lib ' .. api.version .. ': ' .. events[1].data)
  return
end
local reply = api.fetch(arg[1], {headers={Accept='application/json'}})
print('HTTP ' .. reply.status .. ' (' .. #reply.body .. ' bytes)')
