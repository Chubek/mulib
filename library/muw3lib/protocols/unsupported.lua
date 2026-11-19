local M = {}
local missing = {
  websocket='lmuw3 exposes HTTP request/response, not a WebSocket upgrade or duplex socket',
  grpc='gRPC requires HTTP/2 framing and Protobuf; lmuw3 exposes neither',
  webrtc='WebRTC needs a peer-connection/media runtime',
}
function M.reason(name) return missing[name] end
function M.require(name)
  error(name .. ' is unavailable: ' .. (missing[name] or 'no runtime implementation'), 2)
end
return M
