-- muw3 --weblet library/lmuw3/examples/request.lua http://localhost:8000/
local w = require('lmuw3')
local response = w.get(assert(arg[1], 'supply a URL'))
print(response.status, response.url)
io.write(response.body)
