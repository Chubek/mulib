-- Deterministic Lua-side regression tests. Native hooks below are deliberate
-- stubs, not renderer/backend validation. Also runnable by Mune --run/--vm.
package.path = (immgtk_library or 'library') .. '/?.lua;' ..
               (immgtk_library or 'library') .. '/?/init.lua;' .. package.path
local old_native = {
  imm_rect, imm_text, imm_pointer, imm_interact,
  imm_line, imm_clip_push, imm_clip_pop
}
local draws, captions, interactions, seen
local clip_depth, pushes, pops = 0, 0, 0
local held, focused, clicked, fail_at, rect_hook, held_hook
local mx, my = 75, 10
local function reset()
  draws, captions, interactions, seen = {}, {}, {}, {}
  held, focused, clicked, fail_at, rect_hook, held_hook = false, false, false
end
reset()
function imm_rect(x, y, w, h, color)
  assert(type(x) == 'number' and x == x and math.abs(x) <= 1000000)
  assert(type(y) == 'number' and y == y and math.abs(y) <= 1000000)
  assert(w >= 0 and h >= 0 and w <= 1000000 and h <= 1000000)
  assert(color >= 0 and color <= 0xffffffff and color == math.floor(color))
  draws[#draws + 1] = {x, y, w, h, color}
  if rect_hook then rect_hook() end
end
function imm_text(text, x, y, color)
  assert(type(text) == 'string')
  captions[#captions + 1] = text
end
function imm_pointer() return mx, my end
function imm_interact(id, x, y, w, h)
  assert(id > 0 and id <= 9007199254740991 and id == math.floor(id))
  assert(not seen[id], 'duplicate native ID')
  seen[id] = true
  interactions[#interactions + 1] = id
  if #interactions == fail_at then error('injected interaction failure', 0) end
  return true, held_hook and held_hook(#interactions) or held, clicked, focused
end
local function push_clip()
  clip_depth, pushes = clip_depth + 1, pushes + 1
end
local function pop_clip()
  assert(clip_depth > 0, 'native clip underflow')
  clip_depth, pops = clip_depth - 1, pops + 1
end
imm_clip_push, imm_clip_pop, imm_line = push_clip, pop_clip, nil
local function fails(fn, message)
  local ok, err = pcall(fn)
  assert(not ok, 'invalid operation accepted')
  if message then assert(string.find(tostring(err), message, 1, true), tostring(err)) end
  return err
end
local ui = require('immgtk')
local legacy_button = ui.button
local a = require('immgtk.extensions')
assert(a ~= ui and require('immgtk') == ui)
assert(require('immgtk.button') == ui and require('immgtk.image') == ui)
assert(require('immgtk.controls.input_text') == a and
       require('immgtk.collections.list_view') == a)
assert(ui.button == legacy_button and ui.input_text == nil)

-- Legacy compatibility fixes: validation precedes drawing/interaction/layout.
ui.layout(1, 2, 100, 20, 3)
fails(function() ui.image({0xff112233, -1}, 2, 1) end)
fails(function() ui.image({0xff112233, 0/0}, 2, 1) end)
fails(function() ui.image({0xff112233, .5}, 2, 1) end)
fails(function() ui.image({0xff112233, 'color'}, 2, 1) end)
local length_calls = 0
fails(function()
  ui.image(setmetatable({0xff112233, -1}, {__len = function()
    length_calls = length_calls + 1
    return length_calls == 1 and 2 or 1
  end}), 2, 1)
end)
assert(#draws == 0 and ui.ly == 2)
fails(function() ui.slider(1, 0/0, 0, 1) end)
fails(function() ui.vslider(1, 0, 1, 1) end)
assert(#interactions == 0 and ui.ly == 2)
for _, name in ipairs({'button', 'selectable', 'menu_item', 'collapsing_header'}) do
  fails(function() ui[name](1, {}, true, 0, 0, 100, 20) end)
end
assert(#draws == 0 and #interactions == 0)
assert(not ui.button(1, 'ID is still available', 0, 0, 100, 20))
reset()
local pixels = {0xff112233, 0xff445566}
rect_hook = function() pixels[2] = -1 end
ui.image(pixels, 2, 1, 0, 0, 20, 10)
assert(#draws == 2 and draws[1][3] == 10 and draws[2][1] == 10)
assert(draws[2][5] == 0xff445566, 'draw must use validated color snapshot')
reset()
held = true
local value, changed = ui.slider(1, 0, 0, 1, 0, 0, 100, 20)
assert(value == .75 and changed)
value, changed = ui.vslider(2, 0, 0, 1, 0, 0, 100, 20)
assert(value == .5 and changed)

-- Typed, length-delimited IDs and finite, bounded input keys.
reset()
a.begin_frame()
local stable = a.id('stable')
assert(a.id('stable') == stable and a.id(1) == a.id(1.0))
assert(a.id(1) ~= a.id('1') and a.id(false) ~= a.id('false'))
a.push_id('a:b')
local scoped = a.id('c')
a.pop_id()
a.push_id('a')
assert(a.id('b:c') ~= scoped)
a.pop_id()
fails(function() a.pop_id() end)
local tostring_called = false
fails(function()
  a.id(setmetatable({}, {__tostring = function() tostring_called = true end}))
end)
assert(not tostring_called)
fails(function() a.id(math.huge) end)
fails(function() a.id(string.rep('x', 1025)) end)
for i = 1, 64 do a.push_id(i) end
fails(function() a.push_id(65) end)
for i = 1, 64 do a.pop_id() end
local next_id, layout_y = a.next_id, ui.ly
fails(function() a.hit('bad-box', 0, 0, -1, 20) end)
fails(function() a.button('bad-label', {}, 0, 0, 100, 20) end)
fails(function() a.slider('bad-value', 0/0, 0, 1) end)
assert(a.next_id == next_id and ui.ly == layout_y and #interactions == 0)
a.end_frame()

-- begin_frame snapshots only supported scalar fields, without changing a
-- previous valid frame on validation failure.
local input = {dx = 3, dy = -2, text = 'X', backspace = true}
a.begin_frame(input)
local snapshot = a.input
input.dx, input.text = 900, 'mutated'
assert(a.input.dx == 3 and a.input.text == 'X')
fails(function() a.begin_frame({dx = 0/0}) end)
fails(function() a.begin_frame({backspace = 0}) end)
fails(function() a.begin_frame({text = string.rep('a', 4097)}) end)
assert(a.input == snapshot)
local bad_utf8 = {
  string.char(128), string.char(192, 128), string.char(226, 130),
  string.char(237, 160, 128), string.char(244, 144, 128, 128)
}
for i = 1, #bad_utf8 do
  fails(function() a.begin_frame({text = bad_utf8[i]}) end)
  assert(a.input == snapshot)
end
reset()
focused = true
local emoji = string.char(240, 159, 152, 128)
value, changed = a.input_text('text-1', 'A' .. emoji, 0, 0, 100, 20)
assert(value == 'AX' and changed, 'backspace removes one UTF-8 scalar')
value, changed = a.input_text('text-2', 'B', 0, 20, 100, 20)
assert(value == 'B' and not changed, 'input pulse consumed only once')
a.begin_frame({text = emoji})
reset()
value = a.input_text('unfocused', 'A', 0, 0, 100, 20)
assert(value == 'A')
focused = true
value = a.input_text('focused', 'A', 0, 20, 100, 20)
assert(value == 'A' .. emoji)
a.begin_frame({text = 'x'})
reset()
fails(function() a.input_text('too-long', string.rep('a', 65536), 0, 0, 100, 20) end)
fails(function() a.input_text('bad-utf8', bad_utf8[4], 0, 0, 100, 20) end)
fails(function() a.input_number('bad-number', math.huge, '1', 0, 0, 100, 20) end)
assert(#interactions == 0 and #draws == 0)
a.begin_frame()
focused = true
local number, buffer, number_changed = a.input_number('number', 2, '1e999', 0, 0, 100, 20)
assert(number == 2 and buffer == '1e999' and not number_changed)

-- Fallback drawing is bounded; native line dispatch preserves its signature.
reset()
a.line(-1000000, 0, 1000000, 0, 0xffffffff)
assert(#draws == 2049 and draws[1][1] == -1000000 and
       draws[#draws][1] == 1000000)
reset()
fails(function() a.line(0/0, 0, 1, 1, 0) end)
fails(function() a.line(0, 0, 1, 1, -1) end)
assert(#draws == 0)
local native_lines = 0
imm_line = function(x1, y1, x2, y2, width, color)
  assert(width == 1 and color == 0xff112233)
  native_lines = native_lines + 1
end
a.line(0, 0, 5, 5, 0xff112233)
assert(native_lines == 1 and #draws == 0)
imm_line = nil

-- Missing/partial hook pairs must never push an unpoppable native clip.
local before_pushes = pushes
imm_clip_pop = nil
assert(a.clip(0, 0, 10, 10) == false and pushes == before_pushes)
fails(function() a.clip(0, 0, -1, 10) end)
a.unclip(false)
imm_clip_pop = pop_clip
assert(a.clip(0, 0, 10, 10) == true)
imm_clip_pop = function() error('replacement hook must not be called') end
a.unclip(true)
assert(clip_depth == 0, 'pop hook is pinned to its matching push')
imm_clip_pop = pop_clip
fails(function() a.unclip(true) end)
for i = 1, 64 do assert(a.clip(0, 0, 10, 10)) end
fails(function() a.clip(0, 0, 10, 10) end)
for i = 1, 64 do a.unclip(true) end
assert(clip_depth == 0)
imm_clip_push = function() return false end
fails(function() a.clip(0, 0, 10, 10) end)
assert(clip_depth == 0)
imm_clip_push = push_clip

-- Errors preserve their original object and unwind clips, IDs and layout.
ui.layout(7, 9, 100, 20, 3)
local marker = {}
assert(fails(function()
  a.with_id('outer', function()
    a.with_clip(0, 0, 10, 10, function(pushed)
      assert(pushed)
      a.push_id('forgotten')
      assert(a.clip(1, 1, 2, 2))
      ui.layout(0, 0, 5)
      error(marker, 0)
    end)
  end)
end) == marker)
assert(#a.scope == 0 and clip_depth == 0 and ui.lx == 7 and ui.ly == 9)
local one, two, three = a.with_id('returns', function() return 1, nil, 3 end)
assert(one == 1 and two == nil and three == 3)
assert(select('#', a.with_id('trailing-nil', function() return 1, nil end)) == 2)
a.push_id('caller')
assert(a.clip(0, 0, 10, 10))
fails(function() a.with_id('guarded', function() a.pop_id(); a.pop_id() end) end)
fails(function() a.with_id('guarded', function() a.unclip(true) end) end)
assert(#a.scope == 1 and clip_depth == 1)
a.unclip(true); a.pop_id()
-- Native pop failure is retryable and cannot mask the callback's error.
local pop_fails = true
imm_clip_pop = function()
  if pop_fails then return false end
  pop_clip()
end
assert(fails(function()
  a.with_clip(0, 0, 10, 10, function() error(marker, 0) end)
end) == marker)
assert(clip_depth == 1)
pop_fails = false
fails(function() a.end_frame() end, 'unbalanced')
assert(clip_depth == 0)
imm_clip_pop = pop_clip
a.begin_frame()

-- Containers are LIFO across kinds; failed setup or callback restores layout.
reset()
fails(function() a.begin_window('bad-window', {}, {x=0,y=0,w=100,h=100}) end)
fails(function() a.begin_window('small-window', 'x', {x=0,y=0,w=16,h=100}) end)
assert(#a.scope == 0 and #interactions == 0)
ui.layout(7, 9, 100, 20, 3)
local rect = {x=10,y=10,w=150,h=120}
a.begin_window('window', 'Window', rect)
local child = a.begin_child('child', 20, 50, 100, 50)
fails(function() a.end_window() end, 'order')
fails(function() a.end_child({}) end, 'order')
assert(#a.scope == 2 and clip_depth == 2)
a.end_child(child)
a.end_window()
assert(#a.scope == 0 and #a.windows == 0 and clip_depth == 0)
assert(ui.lx == 7 and ui.ly == 9)
fails(function() a.end_child(child) end)
imm_clip_push = function() error('injected push failure', 0) end
fails(function() a.begin_child('failed-child', 0, 0, 100, 50) end, 'push failure')
assert(#a.scope == 0 and clip_depth == 0 and ui.ly == 9)
imm_clip_push = push_clip
reset()
assert(fails(function()
  a.frame({}, function()
    a.begin_window('failed-frame-window', 'x', rect)
    a.begin_child('failed-frame-child', 20, 50, 100, 50)
    error(marker, 0)
  end)
end) == marker)
assert(#a.scope == 0 and #a.windows == 0 and clip_depth == 0 and ui.ly == 9)
a.frame({}, function()
  a.begin_child('normal-frame-child', 0, 0, 100, 50)
  -- The callback guard closes even an accidentally omitted explicit end.
end)
assert(clip_depth == 0 and #a.scope == 0 and ui.ly == 9)
a.begin_frame({dx=5})
reset()
held = true
rect_hook = function() error('injected draw failure', 0) end
fails(function() a.begin_window('moving-window', 'x', rect) end, 'draw failure')
assert(rect.x == 10 and rect.y == 10 and #a.scope == 0)
reset()
held = true
a.begin_window('moving-window', 'x', rect)
a.end_window()
assert(rect.x == 15 and rect.y == 10)
-- Explicit end_frame reports an unclosed container, but still unwinds it.
a.begin_child('leaked-child', 0, 0, 100, 50)
fails(function() a.end_frame() end, 'unbalanced')
assert(clip_depth == 0 and #a.scope == 0 and ui.ly == 9)
a.begin_frame()

-- All vector/range inputs are checked before the first component; model
-- writes are staged until every native interaction/draw succeeds.
reset()
local vector = {.2, 0/0}
fails(function() a.drag_vector('invalid-vector', vector, 0, 1, 0, 0, 100, 20) end)
assert(vector[1] == .2 and #interactions == 0 and #draws == 0 and #a.scope == 0)
fails(function() a.range_slider('invalid-range', .2, 0/0, 0, 1, 0, 0, 100, 40) end)
fails(function() a.color_picker('invalid-color', -1, 0, 0, 100, 40) end)
assert(#interactions == 0 and #draws == 0 and #a.scope == 0)
ui.layout(0, 9, 1, 20)
fails(function() a.drag_vector('narrow-vector', {.2, .3}, 0, 1) end)
assert(ui.ly == 9 and #interactions == 0)
ui.layout(7, 9, 100, 20, 3)
vector = {.2, .3}
held, fail_at = true, 2
fails(function() a.drag_vector('vector', vector, 0, 1, 0, 0, 100, 20) end,
      'interaction failure')
assert(vector[1] == .2 and vector[2] == .3 and #a.scope == 0)
reset()
held = true
local returned
returned, changed = a.drag_vector('vector', vector, 0, 1, 0, 0, 100, 20)
assert(returned == vector and changed and vector[1] == 1 and vector[2] == .5)
reset()
held_hook = function(i) return i == 1 end
mx = 100
value, changed = a.color_picker('color', 0xff112233, 0, 0, 100, 40)
assert(value == 0xff112233 and not changed)
mx = 75

-- Virtualization rejects nonfinite/fractional inputs and empty viewports.
local first, last = a.virtual_list(10, 5, 0, 10)
assert(first == 1 and last == 0)
first, last = a.virtual_list(0, 0, 100, 10)
assert(first == 1 and last == 0)
first, last = a.virtual_list(10, 10000, 100, 10)
assert(first == 11 and last == 10)
first, last = a.virtual_list(10, 5, 20, 10)
assert(first == 1 and last == 3)
fails(function() a.virtual_list(1.5, 0, 20, 10) end)
fails(function() a.virtual_list(10, 0/0, 20, 10) end)
fails(function() a.virtual_list(10, 0, 20, 0) end)
reset()
fails(function() a.list_view('list', {'one', {}}, 1, 0, 0, 0, 100, 20, 10) end)
assert(#interactions == 0 and #draws == 0 and clip_depth == 0 and #a.scope == 0)
clicked = true
value, changed = a.list_view('list', {'one', 'two', 'three'}, 1, 10, 0, 0, 100, 10, 10)
assert(value == 2 and changed and #interactions == 1 and clip_depth == 0)
a.begin_frame({scroll=1})
reset()
local offset = a.scroll_area('scroll', 50, 200, 0, 0, 100, 50,
  function(x, y, w, h, scrolled) assert(y == -26 and scrolled == 26) end)
assert(offset == 26 and clip_depth == 0 and #a.scope == 0)
assert(fails(function()
  a.scroll_area('scroll-error', 50, 200, 0, 0, 100, 50,
    function() ui.layout(0, 0, 20); error(marker, 0) end)
end) == marker)
assert(clip_depth == 0 and #a.scope == 0 and ui.ly == 9)
a.end_frame()
reset()
local demo_state = {bounds={x=0,y=0,w=150,h=120},gain=.5,time=10}
assert(a.demo(demo_state, {}) == demo_state)
assert(clip_depth == 0 and #a.scope == 0 and #a.windows == 0 and ui.ly == 9)
reset()
demo_state.time = 0/0
fails(function() a.demo(demo_state, {}) end)
assert(clip_depth == 0 and #a.scope == 0 and #a.windows == 0 and ui.ly == 9)
assert(pushes == pops, 'all successful native pushes must be balanced')

imm_rect, imm_text, imm_pointer, imm_interact = old_native[1], old_native[2],
                                              old_native[3], old_native[4]
imm_line, imm_clip_push, imm_clip_pop = old_native[5], old_native[6], old_native[7]
print('ImmGTK Lua overlay regression: OK (stub-native hooks)')
