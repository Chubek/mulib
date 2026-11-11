-- Start the worker first. Supply socket path, URL and a unique client topic suffix.
local w = require('lmuw3')
local bus = w.channel(assert(arg[1]))
local reply = 'muw3/fetch/reply/' .. assert(arg[3], 'supply unique client name')
bus.subscribe(reply)
bus.publish('muw3/fetch/request', w.json_encode{url=assert(arg[2]), reply=reply})
local event = assert(bus.receive(30000), 'worker timed out')
print(event.body)
bus.close()
