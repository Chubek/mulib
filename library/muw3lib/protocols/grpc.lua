-- Exported for discovery only, fails fast rather than faking gRPC semantics.
local unsupported = require('muw3lib.protocols.unsupported')
local M = {supported=false}
function M.new() return unsupported.require('grpc') end
return M
