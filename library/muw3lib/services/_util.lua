-- Validation shared by the service convenience methods (not the raw client).
local c = require('muw3lib.core')
local M = {}

function M.object(value, label, optional)
  if value == nil and optional then return {} end
  assert(type(value) == 'table' and value ~= c.muw3.null, label..' must be an object')
  local copy = {}
  for key,v in next,value do
    assert(type(key) == 'string' and #key > 0, label..' must have nonempty string keys')
    copy[key] = v
  end
  return copy
end

function M.text(value, label)
  assert(type(value) == 'string' and value:find('%S') and not value:find('%z'), 'invalid '..label)
  return value
end

function M.token(value, label)
  M.text(value, label)
  assert(not value:find('[%s%c]'), 'invalid '..label)
  return value
end

function M.header(value, label)
  M.text(value, label)
  assert(not value:find('%c'), 'invalid '..label)
  return value
end

-- A base override is an explicit caller choice of trusted server. Only
-- OpenCode permits cleartext, and only to an exact loopback IP literal.
function M.base(value, default, loopback)
  if value == nil then return default end
  assert(type(value) == 'string', 'service base_url must be a string')
  local scheme, authority = value:match('^([A-Za-z]+)://([^/?#]+)')
  if scheme and scheme:lower() == 'https' then return value end
  if loopback and scheme and scheme:lower() == 'http' then
    if authority == '127.0.0.1' or authority == '[::1]'
        or authority:match('^127%.0%.0%.1:[0-9]+$')
        or authority:match('^%[::1%]:[0-9]+$') then return value end
  end
  error('service base_url requires HTTPS'..(loopback and ' or a loopback IP' or ''), 2)
end

function M.id(value, label, pattern)
  M.text(value, label)
  assert(value ~= '.' and value ~= '..' and not value:find('[%c/\\%%]')
    and (not pattern or value:match(pattern)), 'invalid '..label)
  return value
end

function M.positive_id(value, label)
  if type(value) == 'number' then
    local id = math.tointeger(value)
    assert(id and id > 0, 'invalid '..label)
    value = tostring(id)
  end
  -- Keep large decimal identifiers lossless when supplied as strings.
  assert(type(value) == 'string' and value:match('^[0-9]+$') and value:match('[1-9]'),
    'invalid '..label)
  return value
end

function M.path(value, label, pattern, single_segment)
  M.text(value, label)
  assert(value:sub(1,1) ~= '/' and value:sub(-1) ~= '/' and not value:find('//', 1, true),
    'invalid '..label)
  for component in value:gmatch('[^/]+') do M.id(component, label, pattern) end
  if single_segment then return c.encode(value) end
  return c.encode_path(value)
end

function M.extras(required, extra, label, reserved)
  for k,v in pairs(M.object(extra, label, true)) do
    assert(required[k] == nil and not (reserved and reserved[k]), label..' overrides a fixed field')
    required[k] = v
  end
  return required
end

function M.valid_range(value, min, max, label)
  assert(type(value) == 'number' and value >= min and value <= max, 'invalid '..label)
  return value
end

return M
