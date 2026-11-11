-- muw3 --weblet library/lmuw3/examples/worker.lua /absolute/path/bus.sock
local w = require('lmuw3')
local bus = w.channel(assert(arg[1], 'supply MicroChan socket path'))
bus.subscribe('muw3/fetch/request')
while true do
  local event = bus.receive(30000)
  if event then
    local request = w.json_decode(event.body)
    local ok, response = pcall(w.get, request.url)
    local reply = ok and {status=response.status, body=response.body} or {error=tostring(response)}
    -- Reply topic supplied by the application permits concurrent requesters.
    bus.publish(request.reply, w.json_encode(reply))
  end
end
