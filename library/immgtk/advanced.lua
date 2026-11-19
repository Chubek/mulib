local ui = require('immgtk.core')
local a = {
  ids = {}, next_id = 4503599627370496, scope = {}, input = {},
  windows = {}, style = {}
}
local clips, containers = {}, {}
local id_count, id_bytes, guard_depth = 0, 0, 0
local scope_floor, clip_floor, container_floor = 0, 0, 0
local text_used = false
local unpack_values = table.unpack or unpack
local function pack(...) return {n = select('#', ...), ...} end

-- These are hard bounds, not claims about the native host's item capacity.
local MAX_IDS, MAX_DEPTH, MAX_KEY = 65536, 64, 1024
local MAX_ID_BYTES = 8388608
local MAX_TEXT, MAX_INPUT = 65536, 4096

function a._key(key)
  local kind = type(key)
  if kind == 'string' then
    assert(#key <= MAX_KEY, 'ID key too long')
    return 's' .. key
  elseif kind == 'number' then
    ui._number(key)
    assert(math.abs(key) <= 9007199254740991, 'ID number out of range')
    return 'n' .. string.format('%.17g', key == 0 and 0 or key)
  end
  assert(kind == 'boolean', 'ID must be a string, number or boolean')
  return key and 'btrue' or 'bfalse'
end

function a._text(value, limit)
  ui._string(value)
  assert(#value <= (limit or MAX_TEXT), 'text byte limit exceeded')
  -- Strict UTF-8 scalar validation, independent of an optional utf8 library.
  local i = 1
  while i <= #value do
    -- Skip ASCII runs in the runtime, rather than spending a Lua instruction
    -- budget on each byte of a large ordinary text buffer.
    local high_byte = string.find(value, '[\128-\255]', i)
    if not high_byte then break end
    i = high_byte
    local c = string.byte(value, i)
    local n, low, high = 1, 128, 191
    if c >= 194 and c <= 223 then n = 2
    elseif c >= 224 and c <= 239 then
      n = 3
      if c == 224 then low = 160 elseif c == 237 then high = 159 end
    elseif c >= 240 and c <= 244 then
      n = 4
      if c == 240 then low = 144 elseif c == 244 then high = 143 end
    else error('invalid UTF-8', 0) end
    assert(i + n - 1 <= #value, 'truncated UTF-8')
    if n > 1 then
      local second = string.byte(value, i + 1)
      assert(second >= low and second <= high, 'invalid UTF-8')
      for j = 2, n - 1 do
        local nextbyte = string.byte(value, i + j)
        assert(nextbyte >= 128 and nextbyte <= 191, 'invalid UTF-8')
      end
    end
    i = i + n
  end
  return value
end

function a._argb(color)
  ui._number(color)
  assert(color >= 0 and color <= 0xffffffff and color == math.floor(color),
         'invalid ARGB color')
  return color
end

function a._layout()
  return {ui.lx, ui.ly, ui.lw, ui.lr, ui.lg}
end

function a._restore_layout(old)
  ui.lx, ui.ly, ui.lw, ui.lr, ui.lg = old[1], old[2], old[3], old[4], old[5]
end

-- Compound widgets can preflight their derived boxes before committing an
-- implicit layout row. Legacy core/layout behavior is otherwise unchanged.
function a._box(x, y, w, h, validate)
  local implicit = x == nil
  if implicit then
    assert(ui.lx ~= nil, 'call ui.layout first')
    x, y, w, h = ui.lx, ui.ly, ui.lw, ui.lr
  end
  ui._box(x, y, w, h)
  if validate then validate(x, y, w, h) end
  if implicit then ui.ly = y + h + ui.lg end
  return x, y, w, h
end

function a.push_id(key)
  local encoded = a._key(key)
  assert(#a.scope < MAX_DEPTH, 'ID scope depth exceeded')
  a.scope[#a.scope + 1] = encoded
end

function a.pop_id()
  assert(#a.scope > scope_floor, 'ID scope underflow')
  table.remove(a.scope)
end

function a.id(key)
  local encoded = a._key(key)
  local parts = {}
  for i = 1, #a.scope do
    local part = a.scope[i]
    parts[#parts + 1] = #part .. ':' .. part
  end
  parts[#parts + 1] = #encoded .. ':' .. encoded
  local path = table.concat(parts)
  if not a.ids[path] then
    assert(id_count < MAX_IDS and #path <= MAX_ID_BYTES - id_bytes and
           a.next_id < 9007199254740991,
           'extension ID capacity exceeded')
    a.ids[path] = a.next_id
    a.next_id = a.next_id + 1
    id_count = id_count + 1
    id_bytes = id_bytes + #path
  end
  return a.ids[path]
end

function a.clip(x, y, w, h)
  ui._rect(x, y, w, h)
  -- A partial hook pair cannot be balanced and must never be called.
  if type(imm_clip_push) ~= 'function' or type(imm_clip_pop) ~= 'function' then
    return false
  end
  assert(#clips < MAX_DEPTH, 'clip depth exceeded')
  local pop = imm_clip_pop
  assert(imm_clip_push(x, y, w, h) ~= false, 'native clip push failed')
  clips[#clips + 1] = pop
  return true
end

function a.unclip(pushed)
  if not pushed then return end
  assert(pushed == true and #clips > clip_floor, 'clip stack underflow')
  assert(clips[#clips]() ~= false, 'native clip pop failed')
  table.remove(clips)
end

-- Restore structural state on errors, not pixels, native interactions, or
-- arbitrary application state. A failing native pop remains tracked for retry.
function a._guard(draw, keep)
  assert(type(draw) == 'function', 'callback required')
  local depth, clip_depth, count = #a.scope, #clips, #containers
  local old_layout, window_count = a._layout(), #a.windows
  local sf, cf, wf = scope_floor, clip_floor, container_floor
  scope_floor, clip_floor, container_floor = depth, clip_depth, count
  guard_depth = guard_depth + 1
  local result = pack(pcall(draw))
  guard_depth = guard_depth - 1
  scope_floor, clip_floor, container_floor = sf, cf, wf
  if not result[1] or not keep then
    local cleaned = pack(pcall(function()
      while #clips > clip_depth do a.unclip(true) end
    end))
    while #a.scope > depth do table.remove(a.scope) end
    while #containers > count do table.remove(containers) end
    while #a.windows > window_count do table.remove(a.windows) end
    a._restore_layout(old_layout)
    if not result[1] then error(result[2], 0) end
    if not cleaned[1] then error(cleaned[2], 0) end
  end
  if not result[1] then error(result[2], 0) end
  return unpack_values(result, 2, result.n)
end

function a.with_id(key, draw)
  a._key(key)
  assert(type(draw) == 'function', 'callback required')
  return a._guard(function() a.push_id(key); return draw() end)
end

function a.with_clip(x, y, w, h, draw)
  ui._rect(x, y, w, h)
  assert(type(draw) == 'function', 'callback required')
  return a._guard(function() return draw(a.clip(x, y, w, h)) end)
end

-- Internal begin/end containers share one LIFO stack, even across kinds.
function a._open(kind, key, setup)
  a._key(key)
  assert(#containers < MAX_DEPTH, 'container depth exceeded')
  return a._guard(function()
    local state = {kind = kind, old = a._layout(), depth = #a.scope,
                   clips = #clips}
    a.push_id(key)
    state.pushed = setup()
    containers[#containers + 1] = state
    if kind == 'window' then a.windows[#a.windows + 1] = state end
    return state
  end, true)
end

function a._close(kind, state)
  local top = containers[#containers]
  assert(#containers > container_floor and top and top.kind == kind and
         (state == nil or state == top), 'container end order mismatch')
  assert(#a.scope == top.depth + 1 and
         #clips == top.clips + (top.pushed and 1 or 0),
         'unbalanced container scope or clip')
  a.unclip(top.pushed)
  a.pop_id()
  a._restore_layout(top.old)
  table.remove(containers)
  if kind == 'window' then table.remove(a.windows) end
end

function a.begin_frame(input)
  assert(guard_depth == 0 and #a.scope == 0 and #clips == 0 and
         #containers == 0, 'finish the previous extension scopes first')
  if input == nil then input = {} end
  assert(type(input) == 'table', 'input table required')
  local snapshot = {}
  for _, name in ipairs({'dx', 'dy', 'scroll'}) do
    local value = input[name]
    if value == nil then value = 0 end
    ui._number(value)
    assert(math.abs(value) <= 1000000, 'input delta out of range')
    snapshot[name] = value
  end
  for _, name in ipairs({'backspace', 'context_menu'}) do
    local value = input[name]
    assert(value == nil or type(value) == 'boolean', 'boolean input required')
    snapshot[name] = value == true
  end
  snapshot.text = a._text(input.text == nil and '' or input.text, MAX_INPUT)
  a.input, a.windows, a.style = snapshot, {}, {}
  text_used = false
end

function a.end_frame()
  assert(guard_depth == 0, 'cannot end a guarded frame')
  local balanced = #a.scope == 0 and #clips == 0 and #containers == 0
  while #clips > 0 do a.unclip(true) end
  if containers[1] then a._restore_layout(containers[1].old) end
  a.scope, a.windows, containers = {}, {}, {}
  assert(balanced, 'unbalanced extension frame (scopes have been unwound)')
end

function a.frame(input, draw)
  assert(type(draw) == 'function', 'callback required')
  a.begin_frame(input)
  return a._guard(draw)
end

function a.hit(key, x, y, w, h)
  a._key(key)
  x, y, w, h = a._box(x, y, w, h)
  return imm_interact(a.id(key), x, y, w, h)
end

function a.button(key, label, x, y, w, h)
  a._text(label); a._key(key)
  x, y, w, h = a._box(x, y, w, h)
  local hot, held, clicked = a.hit(key, x, y, w, h)
  imm_rect(x, y, w, h, ui._color(hot, held))
  ui._caption(label, x, y)
  return clicked
end

function a.slider(key, value, lo, hi, x, y, w, h)
  ui._range(value, lo, hi); a._key(key)
  x, y, w, h = a._box(x, y, w, h)
  local old = value
  local hot, held = a.hit(key, x, y, w, h)
  if held then
    local mx = imm_pointer()
    ui._number(mx)
    value = lo + (hi - lo) * ui._clamp((mx - x) / w, 0, 1)
  end
  value = ui._clamp(value, lo, hi)
  imm_rect(x, y, w, h, ui._color(hot, false))
  imm_rect(x, y, w * ((value - lo) / (hi - lo)), h, ui.accent)
  return value, value ~= old
end

function a._edit_text(value)
  a._text(value)
  local next_value = value
  if not text_used then
    if a.input.backspace then
      local i = #next_value
      while i > 1 and string.byte(next_value, i) >= 128 and
            string.byte(next_value, i) < 192 do i = i - 1 end
      next_value = string.sub(next_value, 1, i - 1)
    end
    next_value = next_value .. (a.input.text or '')
  end
  return a._text(next_value)
end

function a._consume_text() text_used = true end

function a.line(x1, y1, x2, y2, color)
  ui._rect(x1, y1, 0, 0); ui._rect(x2, y2, 0, 0); a._argb(color)
  if type(imm_line) == 'function' then
    return imm_line(x1, y1, x2, y2, 1, color)
  end
  local n = math.min(2048, math.max(1, math.ceil(
    math.max(math.abs(x2 - x1), math.abs(y2 - y1)))))
  for i = 0, n do
    local t = i / n
    imm_rect(math.floor(x1 + (x2 - x1) * t),
             math.floor(y1 + (y2 - y1) * t), 1, 1, color)
  end
end

function a.row(x, y, w, h, count, index)
  ui._rect(x, y, w, h)
  ui._index(count, 65536); ui._index(index, count)
  local start = math.floor(w * (index - 1) / count)
  return x + start, y, math.floor(w * index / count) - start, h
end

return a
