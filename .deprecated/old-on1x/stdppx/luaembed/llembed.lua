-- Embedded module, evaluated in a fresh restricted Mune state per carrier.
local write_native = __llembed_write
local messages = __llembed_messages
local location, label, input = __llembed_location, __llembed_label, __llembed_input
local serial = __llembed_serial
__llembed_write, __llembed_messages = nil, nil
__llembed_location, __llembed_label, __llembed_input, __llembed_serial = nil, nil, nil, nil

local function fail(key) error(messages[key], 0) end
local function text(value)
  if type(value) ~= "string" or value:find("\0", 1, true) then fail("string") end
  return value
end
local function name(value)
  text(value)
  if #value > 128 or not value:match("^[A-Za-z_][A-Za-z_0-9]*$") then fail("identifier") end
  return value
end
local function quote(value)
  value = text(value)
  local parts = {'"'}
  for i = 1, #value do
    local c = value:byte(i)
    if c == 34 or c == 92 then parts[#parts+1] = "\\" .. string.char(c)
    elseif c >= 32 and c <= 126 then parts[#parts+1] = string.char(c)
    else parts[#parts+1] = string.format("\\x%02x", c) end
  end
  parts[#parts+1] = '"'
  return table.concat(parts)
end
local function sequence(values)
  if type(values) ~= "table" or #values > 4096 then fail("sequence") end
  local n = #values
  for k, v in pairs(values) do
    if type(k) ~= "number" or k % 1 ~= 0 or k < 1 or k > n or type(v) ~= "string" then fail("sequence") end
  end
  for i = 1, n do text(values[i]) end
  return "(" .. table.concat(values, " ") .. ")"
end
local M = { location = location, label = label, input = input, version = 1 }
M.quote = quote
M.symbol = function(value)
  value = text(value)
  if #value == 0 or #value > 128 or value:find('[%s()%c"]') then fail("symbol") end
  for i = 1, #value do if value:byte(i) > 126 then fail("symbol") end end
  return value
end
M.list = sequence
M.node = function(tag, fields)
  name(tag)
  local packed = sequence(fields)
  return "(" .. tag .. (#packed > 2 and " " .. packed:sub(2,-2) or "") .. ")"
end
M.integer = function(value)
  if math.type(value) ~= "integer" then fail("integer") end
  return M.node("DecimalLiteral", {quote(tostring(value)), location})
end
M.string = function(value) return M.node("StringLiteral", {quote(value), location}) end
M.boolean = function(value)
  if type(value) ~= "boolean" then fail("node") end
  return M.node("BooleanLiteral", {tostring(value), location})
end
M.ident = function(value) return M.node("Identifier", {quote(name(value)), location}) end
M.call = function(callee, args) return M.node("Call", {M.ident(callee), sequence(args), location}) end
M.do_ = function(expr) return M.node("Do", {text(expr), location}) end
M.return_ = function(expr) return M.node("Return", {text(expr), location}) end
M.block = function(statements) return M.node("Block", {sequence(statements), location}) end
M.type_ = function(tag) return M.node(name(tag), {}) end
M.function_ = function(identifier, result_type, statements)
  return M.node("Function", {quote(name(identifier)), "()", text(result_type), M.block(statements), "()", location})
end
M.unit = function(declarations) return M.node("Unit", {"1", sequence(declarations)}) end
M.extension = function(operation, source, owner)
  return M.node("ExtensionExpr", {quote(name(owner or label)), quote(text(operation)), sequence({M.string(source)}), location})
end
M.emit = function(value) write_native(value); write_native("\n") end
M.write = function(...)
  local values = {...}
  for i = 1, select("#", ...) do write_native(values[i]) end
end
local fresh_count = 0
M.fresh = function()
  for _ = 1, 256 do
    fresh_count = fresh_count + 1
    if fresh_count > 4096 then fail("fresh") end
    local candidate = name("__llembed_" .. tostring(serial) .. "_" .. tostring(fresh_count))
    if not input:find(quote(candidate), 1, true) then return candidate end
  end
  fail("fresh")
end
-- print/io.write are captured data, never PPX protocol stdout.
print = function(...)
  local values = {...}
  for i = 1, select("#", ...) do
    if i > 1 then write_native("\t") end
    write_native(tostring(values[i]))
  end
  write_native("\n")
end
io = {write = function(...)
  local values = {...}
  for i = 1, select("#", ...) do
    local value = values[i]
    if type(value) ~= "string" and type(value) ~= "number" then fail("string") end
    write_native(tostring(value))
  end
  return true
end}
io.stdout = {write = function(_, ...) return io.write(...) end, flush = function() return true end}
io.flush = function() return true end
require = function(module)
  if module ~= "llembed" then fail("require") end
  return M
end
llembed = M
