-- Private helpers for HTTP protocols with one absolute endpoint.
local core = require('muw3lib.core')
local w = core.muw3
local array_mt = w.array and getmetatable(w.array())
local M = {}

function M.endpoint(endpoint, options)
  local base, path = core.split_url(endpoint)
  assert(path:sub(1, 2) ~= '//', 'endpoint path must begin with a single /')
  return core.new(base, options), path
end

function M.object(value)
  if type(value) ~= 'table' or rawequal(value, w.null) or getmetatable(value) ~= nil then return false end
  for key in next, value do
    if type(key) ~= 'string' then return false end
  end
  return true
end

-- Untagged dense, nonempty lists also work as input to lmuw3. Only the host
-- marker can distinguish an empty JSON array from an empty JSON object.
function M.array_size(value)
  if type(value) ~= 'table' or rawequal(value, w.null) then return nil end
  local mt = getmetatable(value)
  if mt ~= nil and mt ~= array_mt then return nil end
  local count, largest = 0, 0
  for key in next, value do
    if type(key) ~= 'number' or key < 1 or key % 1 ~= 0 then return nil end
    count = count + 1
    if key > largest then largest = key end
  end
  if largest ~= count or (count == 0 and mt == nil) then return nil end
  return count
end

-- Distinguish missing headers from ambiguous singleton headers.
function M.singleton_header(response, name)
  local value
  for _, header in ipairs(response.headers) do
    if header[1]:lower() == name then
      if value ~= nil then return nil, true end
      value = header[2]
    end
  end
  return value, false
end

function M.media_type(response)
  local value, duplicate = M.singleton_header(response, 'content-type')
  if duplicate then return nil, true end
  if value == nil then return nil end
  local mime = value:match('^[ \t]*([^;]+)')
  return mime and mime:gsub('[ \t]+$', ''):lower() or ''
end

function M.flag(options, key)
  if options == nil then return false end
  assert(M.object(options), 'protocol options must be a table')
  local value = options[key]
  assert(value == nil or type(value) == 'boolean', key .. ' must be boolean')
  return value == true
end

return M
