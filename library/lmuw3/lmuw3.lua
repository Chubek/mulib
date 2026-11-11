-- MuW3's Weblet library. Native I/O and data parsers are supplied by MuLIB.
local native = assert(__mulib_muw3, 'lmuw3 requires a MuW3 Mune host')
local M = {version = '1.0'}
local array_mt = {}
M.null = setmetatable({}, {__tostring = function() return 'null' end})
function M.array(t) return setmetatable(t or {}, array_mt) end
local function call(op, ...)
  local value, message, code = native(op, ...)
  if message then error(op .. ': ' .. message .. ' (' .. tostring(code) .. ')', 3) end
  return value
end
local function quote(s)
  return '"' .. s:gsub('[%z\1-\31\\"]', function(c)
    if c == '"' then return '\\"' end
    if c == '\\' then return '\\\\' end
    return string.format('\\u%04x', string.byte(c))
  end) .. '"'
end
local function encode(value)
  local seen = {}
  local function visit(v, depth)
    assert(depth <= 64, 'data nesting limit')
    if v == M.null or v == nil then return 'null' end
    local kind = type(v)
    if kind == 'string' then return quote(v) end
    if kind == 'boolean' then return tostring(v) end
    if kind == 'number' then
      assert(v == v and v ~= math.huge and v ~= -math.huge, 'non-finite number')
      return string.format('%.17g', v)
    end
    assert(kind == 'table' and not seen[v], 'expected acyclic data table')
    seen[v] = true
    local count, maximum, numeric = 0, 0, true
    for k in pairs(v) do
      count = count + 1
      if type(k) ~= 'number' or k < 1 or k % 1 ~= 0 then numeric = false
      elseif k > maximum then maximum = k end
    end
    local array = getmetatable(v) == array_mt or (count > 0 and numeric)
    local parts = {}
    if array then
      assert(numeric and count == maximum, 'arrays must be dense')
      for i = 1, maximum do parts[i] = visit(v[i], depth + 1) end
    else
      local keys = {}
      for k in pairs(v) do assert(type(k) == 'string', 'object keys must be strings'); keys[#keys + 1] = k end
      table.sort(keys)
      for i, k in ipairs(keys) do parts[i] = quote(k) .. ':' .. visit(v[k], depth + 1) end
    end
    seen[v] = nil
    return (array and '[' or '{') .. table.concat(parts, ',') .. (array and ']' or '}')
  end
  return visit(value, 0)
end
function M.json_encode(v) return call('json_encode', encode(v)) end
function M.json_decode(s) return call('json_decode', s) end
function M.yaml_encode(v) return call('yaml_encode', encode(v)) end
function M.yaml_decode(s) return call('yaml_decode', s) end
local function fields(t)
  if not t then return {} end
  if #t > 0 then return t end -- ordered pairs preserve duplicate fields
  local keys, out = {}, {}
  for k, v in pairs(t) do
    assert(type(k) == 'string' and type(v) == 'string', 'fields must contain strings')
    keys[#keys + 1] = k
  end
  table.sort(keys)
  for i, k in ipairs(keys) do out[i] = {k, t[k]} end
  return out
end
function M.form(t) return call('form', fields(t)) end
function M.resolve(base, reference) return call('resolve', base, reference) end
local response = {}
function response:header(name)
  name = name:lower()
  for _, h in ipairs(self.headers) do if h[1]:lower() == name then return h[2] end end
end
function response:json() return M.json_decode(self.body) end
function response:yaml() return M.yaml_decode(self.body) end
function response:expect(status)
  assert(self.status == status, 'HTTP status ' .. self.status .. ', expected ' .. status)
  return self
end
function M.request(spec)
  assert(type(spec) == 'table' and type(spec.url) == 'string', 'request requires url')
  local headers, body, url = fields(spec.headers), spec.body or '', spec.url
  local modes = (spec.body ~= nil and 1 or 0) + (spec.json ~= nil and 1 or 0) + (spec.form ~= nil and 1 or 0)
  assert(modes <= 1, 'choose body, json or form')
  -- Copy ordered headers: requests never mutate the supplied specification.
  local copy = {}
  for i, h in ipairs(headers) do copy[i] = {h[1], h[2]} end
  headers = copy
  local function content_type(value)
    for _, h in ipairs(headers) do if h[1]:lower() == 'content-type' then return end end
    headers[#headers + 1] = {'Content-Type', value}
  end
  if spec.json ~= nil then body = M.json_encode(spec.json); content_type('application/json') end
  if spec.form ~= nil then body = M.form(spec.form); content_type('application/x-www-form-urlencoded') end
  if spec.bearer then headers[#headers + 1] = {'Authorization', 'Bearer ' .. spec.bearer} end
  if spec.query then
    assert(not url:find('#', 1, true), 'query URL must not contain a fragment')
    url = url .. (url:find('?', 1, true) and '&' or '?') .. M.form(spec.query)
  end
  local r = call('request', spec.method or 'GET', url, headers, body)
  return setmetatable(r, {__index = response})
end
function M.get(url, headers) return M.request{url=url, headers=headers} end
function M.post(url, body, headers) return M.request{url=url, method='POST', body=body, headers=headers} end
-- A session supplies a base URL/default headers without hidden global state.
function M.session(base, defaults)
  local S = {}
  function S.request(spec)
    local copy, headers = {}, fields(defaults)
    for k, v in pairs(spec) do copy[k] = v end
    copy.url = M.resolve(base, spec.url)
    copy.headers = {}
    local custom = fields(spec.headers)
    for _, h in ipairs(headers) do
      local overridden = false
      for _, v in ipairs(custom) do if h[1]:lower() == v[1]:lower() then overridden = true end end
      if not overridden then copy.headers[#copy.headers + 1] = h end
    end
    for _, h in ipairs(custom) do copy.headers[#copy.headers + 1] = h end
    return M.request(copy)
  end
  return S
end
-- Collections are ordinary JSON/YAML arrays of request specifications.
function M.collection(specs, after)
  local responses = {}
  for i, spec in ipairs(specs) do
    responses[i] = M.request(spec)
    if after then after(responses[i], spec, i) end
  end
  return responses
end
function M.channel(path)
  local handle, sequence, pending = call('open', path), 0, {}
  local C = {}
  local function receive(ms) return call('receive', assert(handle, 'channel closed'), ms or 30000) end
  local function command(op, topic, body)
    assert(handle, 'channel closed')
    sequence = sequence + 1
    call('send', handle, op, sequence, topic, body or '')
    while true do
      local m = assert(receive(), 'MicroChan acknowledgement timeout')
      if m.opcode == op and m.id == sequence then
        assert(m.status == 0, 'MicroChan status ' .. m.status)
        return C
      end
      assert(#pending < 64, 'MicroChan pending event limit')
      pending[#pending + 1] = m
    end
  end
  function C.subscribe(topic) return command(19713, topic) end
  function C.unsubscribe(topic) return command(19714, topic) end
  function C.publish(topic, body) return command(19715, topic, body) end
  function C.receive(ms)
    if #pending > 0 then return table.remove(pending, 1) end
    return receive(ms)
  end
  function C.close()
    if handle then call('close', handle); handle = nil end
  end
  return C
end
-- These references let the C converter preserve empty arrays and JSON null.
__mulib_muw3_array = array_mt
__mulib_muw3_null = M.null
package.loaded.lmuw3 = M
return M
