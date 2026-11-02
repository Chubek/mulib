local ui = require('immgtk.core')
function ui.tab(id, text, selected, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  assert(h >= 4, 'tab dimensions')
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  imm_rect(x, y, w, h, ui._color(hot, held))
  if selected then imm_rect(x, y + h - 3, w, 3, ui.accent) end
  ui._caption(text, x, y)
  return clicked
end
return ui
