local common = require('muw3lib.protocols.common')
local M = {}

local max_batch = 1024
local max_id = 9007199254740991 -- exact across the host's JSON number conversion
local function message(method, params)
  assert(type(method) == 'string' and method ~= '' and method:sub(1, 4) ~= 'rpc.',
    'JSON-RPC method must be nonempty and outside the reserved rpc. namespace')
  assert(params == nil or common.object(params) or common.array_size(params) ~= nil,
    'JSON-RPC params must be an object or array')
  return {jsonrpc='2.0', method=method, params=params}
end

local function validate(reply)
  assert(common.object(reply) and reply.jsonrpc == '2.0', 'invalid JSON-RPC response version or shape')
  assert((reply.result ~= nil) ~= (reply.error ~= nil),
    'JSON-RPC response must contain exactly one of result or error')
  if reply.error ~= nil then
    local remote = reply.error
    assert(common.object(remote) and type(remote.code) == 'number' and remote.code % 1 == 0
      and type(remote.message) == 'string', 'invalid JSON-RPC error object')
  end
end

function M.new(endpoint, options)
  local client, path = common.endpoint(endpoint, options)
  local obj = {raw=client}
  local headers = {Accept='application/json', ['Content-Type']='application/json'}
  local last_id = 0
  local function spec(payload)
    return {json=payload, headers=headers, allow_error=false}
  end

  local function assign_id(item)
    assert(last_id < max_id, 'JSON-RPC request ID limit reached')
    last_id = last_id + 1
    item.id = last_id
    return last_id
  end

  local function notification(payload)
    local response = client:request('POST', path, spec(payload))
    assert(response.body == '', 'JSON-RPC notification must not receive a response body')
    return response
  end

  function obj.call(method, params, opts)
    local allow_errors = common.flag(opts, 'allow_errors')
    local payload = message(method, params)
    local id = assign_id(payload)
    local reply, response = client:json('POST', path, spec(payload))
    validate(reply)
    assert(reply.id == id, 'JSON-RPC response ID mismatch')
    if reply.error ~= nil and not allow_errors then
      error('JSON-RPC remote error (use allow_errors to inspect the envelope)', 2)
    end
    return reply.result, reply, response
  end

  function obj.notify(method, params)
    return notification(message(method, params))
  end

  -- Replies (including remote errors) are returned in call order, excluding
  -- notifications. Every reply must correlate to exactly one request.
  function obj.batch(entries)
    local count = common.array_size(entries)
    assert(count and count > 0 and count <= max_batch, 'JSON-RPC batch must contain 1-1024 entries')
    local payload, expected, notifications = {}, {}, {}
    for i = 1, count do
      local item = entries[i]
      assert(common.object(item), 'invalid JSON-RPC batch entry')
      assert(item.notify == nil or type(item.notify) == 'boolean', 'JSON-RPC notify flag must be boolean')
      payload[i] = message(item.method, item.params)
      notifications[i] = item.notify == true
    end
    local calls = 0
    for i = 1, count do
      if not notifications[i] then
        calls = calls + 1
        expected[assign_id(payload[i])] = calls
      end
    end
    if calls == 0 then return {}, notification(payload) end

    local replies, response = client:json('POST', path, spec(payload))
    assert(common.array_size(replies) == calls, 'invalid JSON-RPC batch response count or shape')
    local ordered = {}
    for i = 1, calls do
      local reply = replies[i]
      validate(reply)
      local slot = type(reply.id) == 'number' and expected[reply.id]
      assert(slot and ordered[slot] == nil, 'JSON-RPC unexpected or duplicate response ID')
      ordered[slot] = reply
    end
    return ordered, response
  end
  return obj
end

return M
