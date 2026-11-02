-- Executed by test_immgtk.c on the actual Mune runtime, not a substitute Lua.
local ui = require('immgtk')
local names = {
  'panel', 'label', 'button', 'checkbox', 'slider', 'toggle', 'radio',
  'selectable', 'progress', 'vslider', 'scrollbar', 'stepper', 'tab',
  'menu_item', 'link', 'badge', 'separator', 'heading', 'tooltip', 'group_box',
  'collapsing_header', 'tree_node', 'list_box', 'combo_box', 'image',
  'color_swatch', 'sparkline', 'histogram', 'meter', 'spinner'
}
assert(#names == 30)
for i = 1, #names do assert(type(ui[names[i]]) == 'function', names[i]) end

function immgtk_test_draw()
  ui.panel(0, 0, 128, 128)
  ui.label('Label', 2, 2)
  ui.layout(0, 0, 100, 20, 2)
  assert(not ui.button(1, 'Button'))
  local value, changed = ui.checkbox(2, 'Check', false)
  assert(not value and not changed)
  value, changed = ui.slider(3, 0.25, 0, 1)
  assert(value == 0.25 and not changed)
  value, changed = ui.toggle(4, false)
  assert(not value and not changed)
  value, changed = ui.radio(5, 'Radio', 'a', 'b')
  assert(value == 'a' and not changed)
  value, changed = ui.selectable(6, 'Select', true)
  assert(value and not changed)
  ui.progress(2, 0, 1)
  value, changed = ui.vslider(7, 0.25, 0, 1)
  assert(value == 0.25 and not changed)
  value, changed = ui.scrollbar(8, 0.5, 0.2)
  assert(value == 0.5 and not changed)
  value, changed = ui.stepper(9, 0, 1, -10, 10)
  assert(value == 0 and not changed)
  assert(not ui.tab(10, 'Tab', true))
  assert(not ui.menu_item(11, 'Menu', true))
  assert(not ui.link(12, 'Link'))
  ui.badge('Badge')
  ui.separator()
  ui.heading('Heading')
  ui.tooltip('Tooltip', true)
  ui.tooltip('Invisible', false)
  local x, y, w, h = ui.group_box('Group', 0, 0, 100, 40)
  assert(x == 8 and y == 24 and w == 84 and h == 8)
  value, changed = ui.collapsing_header(13, 'Header', true)
  assert(value and not changed)
  value, changed = ui.tree_node(14, 'Node', false, 2)
  assert(not value and not changed)
  value, changed = ui.list_box(15, {'One', 'Two'}, 1)
  assert(value == 1 and not changed)
  value, changed = ui.combo_box(16, {'One', 'Two'}, 1)
  assert(value == 1 and not changed)
  ui.image({0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff}, 2, 2)
  assert(not ui.color_swatch(17, 0xff112233))
  ui.sparkline({-1, 0.5, 2}, 0, 1)
  ui.histogram({-1, 0.5, 2}, 0, 1)
  ui.meter(0.75, 0, 1, 8)
  ui.spinner(0.25)
  -- Saturation, singleton data, full-page scrolling and boundary dimensions.
  value, changed = ui.slider(18, 2, 0, 1, 0, 0, 8, 8)
  assert(value == 1 and changed)
  value, changed = ui.scrollbar(19, 1, 1, 0, 0, 8, 8)
  assert(value == 0 and changed)
  ui.sparkline({0}, 0, 1, 0, 0, 1, 1)
  ui.histogram({1}, 0, 1, 0, 0, 1, 1)
  ui.spinner(-1, 0, 0, 1, 1)
end

function immgtk_test_invalid()
  local function fails(fn) assert(not pcall(fn), 'invalid widget accepted') end
  fails(function() ui.layout(0, 0, 10, 0) end)
  fails(function() ui.layout(0, 0, 10, 20, -1) end)
  fails(function() ui.panel(0/0, 0, 10, 10) end)
  fails(function() ui.panel(0, 0, -1, 1) end)
  fails(function() ui.label({}, 0, 0) end)
  fails(function() ui.button(100, {}, 0, 0, 100, 20) end)
  fails(function() ui.checkbox(100, 'x', false, 0, 0, 8, 20) end)
  fails(function() ui.slider(100, 0, 1, 1, 0, 0, 100, 20) end)
  fails(function() ui.slider(100, math.huge, 0, 1, 0, 0, 100, 20) end)
  fails(function()
    ui.slider(100, 0, math.mininteger, math.maxinteger, 0, 0, 100, 20)
  end)
  fails(function() ui.toggle(100, false, 0, 0, 20, 20) end)
  fails(function() ui.radio(100, 'x', 0, nil, 0, 0, 100, 20) end)
  fails(function() ui.selectable(100, {}, false, 0, 0, 100, 20) end)
  fails(function() ui.progress(0, 1, 0, 0, 0, 100, 20) end)
  fails(function() ui.vslider(100, 0, 0, 1, 0, 0, 1, 1) end)
  fails(function() ui.scrollbar(100, 0, 0, 0, 0, 100, 20) end)
  fails(function() ui.stepper(100, 0, -1, 0, 1, 0, 0, 100, 20) end)
  fails(function() ui.tab(100, 'x', false, 0, 0, 100, 1) end)
  fails(function() ui.menu_item(100, {}, true, 0, 0, 100, 20) end)
  fails(function() ui.link(100, 'x', 0, 0, 100, 1) end)
  fails(function() ui.badge({}, 0, 0, 100, 20) end)
  fails(function() ui.separator(0, 0, 0, 20) end)
  fails(function() ui.heading('x', 0, 0, 100, 1) end)
  fails(function() ui.tooltip({}, true, 0, 0, 100, 20) end)
  fails(function() ui.group_box('x', 0, 0, 8, 8) end)
  fails(function() ui.collapsing_header(100, {}, false, 0, 0, 100, 20) end)
  fails(function() ui.tree_node(100, 'x', false, 1.5, 0, 0, 100, 20) end)
  fails(function() ui.list_box(100, {}, 1, 0, 0, 100, 20) end)
  fails(function() ui.combo_box(100, {'a'}, 0, 0, 0, 100, 20) end)
  fails(function() ui.image({0}, 2, 2, 0, 0, 100, 20) end)
  fails(function() ui.image({-1}, 1, 1, 0, 0, 100, 20) end)
  fails(function() ui.color_swatch(100, -1, 0, 0, 100, 20) end)
  fails(function() ui.sparkline({}, 0, 1, 0, 0, 100, 20) end)
  fails(function() ui.histogram({0/0}, 0, 1, 0, 0, 100, 20) end)
  fails(function() ui.meter(0, 0, 1, 0, 0, 0, 100, 20) end)
  fails(function() ui.spinner(math.huge, 0, 0, 100, 20) end)
  -- Failed validation must not have consumed the interaction ID.
  assert(not ui.button(100, 'Still usable', 0, 0, 100, 20))
  assert(not ui.menu_item(101, 'Disabled', false, 0, 0, 100, 20))
end

function immgtk_test_interact(case, down)
  -- All controls use ID 200, submitted once per frame. A new press follows
  -- each release; the C driver positions the pointer at (75,10).
  local value, changed
  if case == 1 then
    assert(ui.button(200, 'Button', 0, 0, 100, 20) == not down)
  elseif case == 2 then
    value, changed = ui.checkbox(200, 'Check', false, 0, 0, 100, 20)
    assert(value == not down and changed == not down)
  elseif case == 3 then
    value, changed = ui.slider(200, 0, 0, 1, 0, 0, 100, 20)
    assert(value == (down and 0.75 or 0) and changed == down)
  elseif case == 4 then
    value, changed = ui.toggle(200, false, 0, 0, 100, 20)
    assert(value == not down and changed == not down)
  elseif case == 5 then
    value, changed = ui.radio(200, 'Radio', 'a', 'b', 0, 0, 100, 20)
    assert(value == (down and 'a' or 'b') and changed == not down)
  elseif case == 6 then
    value, changed = ui.selectable(200, 'Select', false, 0, 0, 100, 20)
    assert(value == not down and changed == not down)
  elseif case == 7 then
    value, changed = ui.vslider(200, 0, 0, 1, 0, 0, 100, 20)
    assert(value == (down and 0.5 or 0) and changed == down)
  elseif case == 8 then
    value, changed = ui.scrollbar(200, 0, 0.2, 0, 0, 100, 20)
    assert(value == (down and 0.5 or 0) and changed == down)
  elseif case == 9 then
    value, changed = ui.stepper(200, 0, 1, -10, 10, 0, 0, 100, 20)
    assert(value == (down and 0 or 1) and changed == not down)
  elseif case == 10 then
    assert(ui.tab(200, 'Tab', false, 0, 0, 100, 20) == not down)
  elseif case == 11 then
    assert(ui.menu_item(200, 'Menu', true, 0, 0, 100, 20) == not down)
  elseif case == 12 then
    assert(ui.link(200, 'Link', 0, 0, 100, 20) == not down)
  elseif case == 13 then
    value, changed = ui.collapsing_header(200, 'Header', false, 0, 0, 100, 20)
    assert(value == not down and changed == not down)
  elseif case == 14 then
    value, changed = ui.tree_node(200, 'Node', false, 0, 0, 0, 100, 20)
    assert(value == not down and changed == not down)
  elseif case == 15 then
    value, changed = ui.list_box(200, {'a', 'b'}, 1, 0, 0, 100, 20)
    assert(value == (down and 1 or 2) and changed == not down)
  elseif case == 16 then
    value, changed = ui.combo_box(200, {'a', 'b'}, 1, 0, 0, 100, 20)
    assert(value == (down and 1 or 2) and changed == not down)
  elseif case == 17 then
    assert(ui.color_swatch(200, 0xff112233, 0, 0, 100, 20) == not down)
  else error('unknown interaction case') end
end
