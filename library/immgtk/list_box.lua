local ui = require('immgtk.core')
-- One interaction ID for the entire list, not generated IDs for its rows.
function ui.list_box(id, items, selected, x, y, w, h)
  ui._items(items); ui._index(selected, #items)
  x, y, w, h = ui._box(x, y, w, h)
  assert(h >= #items * 8, 'list rows too small')
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  local old = selected
  local row = h / #items
  if clicked then
    local mx, my = imm_pointer()
    selected = ui._clamp(math.floor((my - y) / row) + 1, 1, #items)
  end
  for i = 1, #items do
    local yy = y + (i - 1) * row
    imm_rect(x, yy, w, row, i == selected and ui.accent or ui.normal)
    ui._caption(items[i], x, yy)
  end
  return selected, selected ~= old
end
return ui
