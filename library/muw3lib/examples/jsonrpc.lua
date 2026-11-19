-- build/muw3 --weblet library/muw3lib/examples/jsonrpc.lua URL METHOD [JSON_PARAMS]
-- Use only an endpoint you trust. Omit JSON_PARAMS for a request without params.
package.path = 'library/?.lua;library/?/init.lua;' .. package.path
local api = require('muw3lib')
local rpc = api.jsonrpc.new(assert(arg[1], 'JSON-RPC endpoint required'))
local params
if arg[3] ~= nil then params = api.core.muw3.json_decode(arg[3]) end
local value = rpc.call(assert(arg[2], 'method required'), params)
print(api.core.muw3.json_encode(value))
