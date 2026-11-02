local ui = require('immgtk.core')
-- Compact cycling selector; intentionally not a popup/dropdown implementation.
function ui.combo_box(id, items, selected, x, y, w, h)
  ui._items(items); ui._index(selected, #items)
  x, y, w, h = ui._box(x, y, w, h)
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  local old = selected
  if clicked then selected = selected % #items + 1 end
  imm_rect(x, y, w, h, ui._color(hot, held))
  ui._caption(items[selected] .. ' >', x, y)
  return selected, selected ~= old
end
return ui
