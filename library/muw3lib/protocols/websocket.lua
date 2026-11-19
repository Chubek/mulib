-- Exported for discovery only, fails fast rather than faking WebSocket semantics.
local unsupported = require('muw3lib.protocols.unsupported')
local M = {supported=false}
function M.new() return unsupported.require('websocket') end
return M
