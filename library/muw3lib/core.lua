-- Shared MuW3 HTTP transport for Mune; no external Lua/curl dependencies.
local w = require('lmuw3')
local M = {muw3 = w}
local function text(v)
  if type(v) == 'boolean' then return v and 'true' or 'false' end
  return tostring(v)
end
local function scalar(v)
  local kind = type(v)
  return kind == 'string' or kind == 'boolean'
    or (kind == 'number' and v == v and v ~= math.huge and v ~= -math.huge)
end
function M.encode(value)
  assert(scalar(value), 'path component must be a string, boolean, or finite number')
  local s = text(value)
  return (s:gsub('[^A-Za-z0-9%-._~]', function(c)
    return string.format('%%%02X', string.byte(c))
  end))
end
function M.encode_path(value)
  assert(scalar(value), 'path must be a string, boolean, or finite number')
  local path = text(value)
  return (path:gsub('[^/]+', M.encode))
end
local MAX_FIELDS = 256 -- Mune bridge limit; the HTTP engine may impose a lower limit.

-- A field collection is either an all-string-key map or a dense array of pairs.
local function field_list(t, what)
  assert(type(t) == 'table', what .. ' must be a map or ordered pairs')
  local keys, count, numeric = {}, 0, nil
  for k in next, t do
    local n = type(k) == 'number'
    assert(n or type(k) == 'string', what .. ' keys must be strings or dense indices')
    assert(numeric == nil or numeric == n, what .. ' cannot mix map keys and indices')
    numeric = n
    count = count + 1
    assert(count <= MAX_FIELDS, what .. ' field limit exceeded')
    if not n then keys[#keys + 1] = k end
  end
  if numeric then
    for i = 1, count do
      local p = rawget(t, i)
      assert(type(p) == 'table' and rawget(p, 1) ~= nil and rawget(p, 2) ~= nil,
        what .. ' must be dense name/value pairs')
      for k in next, p do
        assert(k == 1 or k == 2, what .. ' entries must contain exactly two fields')
      end
    end
  else
    table.sort(keys)
  end
  return numeric, count, keys
end
local function query_text(v)
  assert(scalar(v), 'query/form fields must be strings, booleans, or finite numbers')
  local s = text(v)
  assert(not s:find('%z'), 'query/form fields cannot contain NUL')
  return s
end
function M.qvalues(t)
  if t == nil then return nil end
  local ordered, count, keys = field_list(t, 'query/form')
  local out = {}
  if ordered then
    for i = 1, count do
      local p = rawget(t, i)
      out[i] = {query_text(rawget(p, 1)), query_text(rawget(p, 2))}
    end
  else
    for _, k in ipairs(keys) do out[query_text(k)] = query_text(rawget(t, k)) end
  end
  return out
end
function M.xml_escape(v)
  return (text(v):gsub('[&<>"\']', {['&']='&amp;', ['<']='&lt;', ['>']='&gt;', ['"']='&quot;', ["'"]='&apos;'}))
end
function M.base64(input)
  assert(type(input) == 'string', 'base64 expects string')
  local chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
  local out = {}
  for i = 1, #input, 3 do
    local a, b, c = input:byte(i, i + 2)
    local n = a * 65536 + (b or 0) * 256 + (c or 0)
    local v1 = math.floor(n / 262144) % 64 + 1
    local v2 = math.floor(n / 4096) % 64 + 1
    local v3 = math.floor(n / 64) % 64 + 1
    local v4 = n % 64 + 1
    out[#out + 1] = chars:sub(v1, v1) .. chars:sub(v2, v2)
      .. (b and chars:sub(v3, v3) or '=') .. (c and chars:sub(v4, v4) or '=')
  end
  return table.concat(out)
end
function M.basic(username, password)
  assert(type(username) == 'string' and type(password) == 'string', 'Basic auth requires strings')
  assert(not username:find(':', 1, true) and not username:find('[%z\1-\31\127]')
    and not password:find('[%z\1-\31\127]'), 'invalid Basic auth credentials')
  return 'Basic ' .. M.base64(username .. ':' .. password)
end

local function url_component(s, pattern)
  return not (s:gsub('%%[A-Fa-f0-9][A-Fa-f0-9]', '')):find(pattern)
end
local function valid_path(path)
  assert(url_component(path, "[^A-Za-z0-9._~!$&'()*+,;=:@/%-]"), 'invalid URL path')
  for segment in path:gmatch('[^/]+') do
    local dots = segment:gsub('%%2[eE]', '.')
    assert(dots ~= '.' and dots ~= '..', 'URL path cannot contain dot segments')
  end
end
local function path_query(tail)
  local sep = tail:find('?', 1, true)
  valid_path(sep and tail:sub(1, sep - 1) or tail)
  if sep then
    assert(url_component(tail:sub(sep + 1), "[^A-Za-z0-9._~!$&'()*+,;=:@/?%-]"), 'invalid URL query')
  end
end
local function absolute_parts(url)
  assert(type(url) == 'string', 'HTTP(S) URL required')
  local scheme, authority, tail = url:match('^([A-Za-z]+)://([^/?#]+)(.*)$')
  assert(scheme and (scheme:lower() == 'http' or scheme:lower() == 'https'), 'HTTP(S) URL required')
  local host, portpart
  if authority:sub(1, 1) == '[' then
    host, portpart = authority:match('^%[([^%]]+)%](.*)$')
    -- Native MuW3 performs full IP-literal validation before connecting.
    assert(host and host:find(':', 1, true) and not host:find('[^0-9A-Fa-f:.]'),
      'invalid URL host')
  else
    host, portpart = authority:match('^([^:]+)(.*)$')
    assert(host and url_component(host, "[^A-Za-z0-9._~!$&'()*+,;=%-]"), 'invalid URL host')
  end
  if portpart ~= '' then
    local digits = portpart:match('^:([0-9]+)$')
    local port = digits and tonumber(digits)
    assert(port and port >= 1 and port <= 65535, 'invalid URL port')
  end
  path_query(tail)
  return scheme .. '://' .. authority, tail
end
-- Split an HTTP(S) endpoint, retaining its path and inline query.
function M.split_url(url)
  local origin, tail = absolute_parts(url)
  assert(tail:sub(1, 2) ~= '//', 'endpoint path cannot begin with //')
  if tail:sub(1, 1) ~= '/' then tail = '/' .. tail end
  return origin, tail
end

local function token(s)
  return type(s) == 'string' and s ~= '' and not s:find("[^A-Za-z0-9!#$%%&'*+.^_`|~%-]")
end
local function header_pair(name, value)
  assert(token(name), 'invalid header name')
  local key = name:lower()
  assert(key ~= 'host' and key ~= 'connection', 'header is managed by the HTTP host')
  assert(type(value) == 'string' and not value:find('[%z\1-\8\10-\31\127]'), 'invalid header value')
  return {name, value}, key
end
local function headers_from(opt)
  local out, seen = {}, {}
  local t = opt.headers
  if t ~= nil then
    local ordered, count, keys = field_list(t, 'headers')
    for i = 1, count do
      local name, value
      if ordered then
        local pair = rawget(t, i)
        name, value = rawget(pair, 1), rawget(pair, 2)
      else
        name, value = keys[i], rawget(t, keys[i])
      end
      local pair, key = header_pair(name, value)
      assert(not seen[key] or (ordered and key ~= 'authorization'),
        'ambiguous header; use ordered pairs for repeatable fields')
      out[#out + 1], seen[key] = pair, true
    end
  end
  assert(opt.bearer == nil or opt.basic == nil, 'choose bearer or basic authentication')
  local credential = opt.bearer or opt.basic
  if opt.bearer ~= nil or opt.basic ~= nil then
    assert(type(credential) == 'string' and credential ~= '', 'authentication must be a non-empty string')
    for i = #out, 1, -1 do
      if out[i][1]:lower() == 'authorization' then table.remove(out, i) end
    end
    local h = header_pair('Authorization', opt.bearer and ('Bearer ' .. credential) or credential)
    out[#out + 1], seen.authorization = h, true
  end
  return out, seen
end
local function request_options(opt)
  if opt == nil then return {} end
  assert(type(opt) == 'table', 'request options must be a table')
  -- Do not silently accept a redirect policy we cannot or will not honor.
  assert(opt.redirect == nil and (opt.follow_redirects == nil or opt.follow_redirects == false),
    'muw3lib requests do not follow redirects')
  return opt
end
local function body_limit(n)
  assert(n == nil or (type(n) == 'number' and n >= 0 and n < math.huge and n % 1 == 0),
    'max_body_bytes must be a non-negative integer')
  return n
end
local function allow_error(v, default)
  assert(v == nil or type(v) == 'boolean', 'allow_error must be boolean')
  if v == nil then return default end
  return v
end
local function perform(method, url, defaults, opt, max_body_bytes, errors_allowed)
  assert(token(method) and method:upper() ~= 'CONNECT', 'invalid HTTP method')
  local limit = body_limit(opt.max_body_bytes)
  if limit == nil then limit = max_body_bytes end
  local accept_errors = allow_error(opt.allow_error, errors_allowed)
  local extras, seen = headers_from(opt)
  local headers = {}
  for _, h in ipairs(defaults) do
    if not seen[h[1]:lower()] then headers[#headers + 1] = {h[1], h[2]} end
  end
  for _, h in ipairs(extras) do headers[#headers + 1] = h end
  local modes = (opt.body ~= nil and 1 or 0) + (opt.json ~= nil and 1 or 0)
    + (opt.form ~= nil and 1 or 0)
  assert(modes <= 1, 'choose body, json or form')
  assert(opt.body == nil or type(opt.body) == 'string', 'body must be a string')
  local count = #headers
  if opt.json ~= nil or opt.form ~= nil then
    local has_type = false
    for _, h in ipairs(headers) do
      if h[1]:lower() == 'content-type' then has_type = true end
    end
    if not has_type then count = count + 1 end
  end
  assert(count <= MAX_FIELDS, 'headers field limit exceeded')
  local query = M.qvalues(opt.query)
  if query ~= nil and next(query) == nil then query = nil end
  if query then
    local first_query = url:find('?', 1, true)
    if first_query == #url or (first_query and url:sub(-1) == '&') then url = url:sub(1, -2) end
  end
  local form = M.qvalues(opt.form)
  assert(w.request_redirect_control == true, 'muw3lib requires lmuw3 request redirect control')
  local response = w.request{url=url, method=method, headers=headers, body=opt.body,
    json=opt.json, form=form, query=query, follow_redirects=false}
  local status = type(response) == 'table' and response.status
  assert(type(status) == 'number' and status >= 100 and status <= 599 and status % 1 == 0
    and type(response.body) == 'string', 'invalid MuW3 response')
  -- The host has already buffered the body; this is an application-level cap.
  if limit and #response.body > limit then error('HTTP response exceeds max_body_bytes', 3) end
  if not accept_errors and (status < 200 or status >= 300) then
    -- Paths, queries, and server-supplied diagnostics can contain credentials.
    error('HTTP ' .. text(status), 3)
  end
  return response
end

local Client = {}
Client.__index = Client
function M.new(base, options)
  local origin, path = absolute_parts(base)
  assert(not path:find('?', 1, true), 'base URL cannot contain a query')
  local opt = request_options(options)
  local headers = headers_from(opt)
  return setmetatable({base=origin .. path:gsub('/+$', ''), headers=headers,
    max_body_bytes=body_limit(opt.max_body_bytes), allow_error=allow_error(opt.allow_error, false)}, Client)
end
function Client:request(method, path, spec)
  assert(type(path) == 'string' and path:sub(1, 1) == '/' and path:sub(1, 2) ~= '//',
    'use a client path beginning with a single /')
  path_query(path)
  return perform(method, self.base .. path, self.headers, request_options(spec),
    self.max_body_bytes, self.allow_error)
end
-- Absolute URL counterpart to Client:request; by default it returns all HTTP statuses.
function M.request(url, spec)
  assert(type(url) == 'string', 'HTTP(S) URL required')
  url = url:match('^[^#]*') -- fragments are client-side only
  absolute_parts(url)
  local opt = request_options(spec)
  local method = opt.method
  if method == nil then method = 'GET' end
  return perform(method, url, {}, opt, nil, true)
end
function Client:json(method, path, spec)
  local r = self:request(method, path, spec)
  if #r.body == 0 then return nil, r end
  local ok, data = pcall(r.json, r)
  if not ok then error('invalid JSON response', 2) end
  return data, r
end
function Client:get(path, query)
  return self:json('GET', path, {query=query})
end
function Client:post(path, payload)
  return self:json('POST', path, {json=payload})
end
M.Client = Client
return M
