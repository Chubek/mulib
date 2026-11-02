local ui = require('immgtk.core')
function ui.checkbox(id, text, value, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  assert(w >= h and h >= 12, 'checkbox dimensions')
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  value = not not value
  if clicked then value = not value end
  imm_rect(x, y, h, h, ui._color(hot, held))
  if value then imm_rect(x + 4, y + 4, h - 8, h - 8, ui.accent) end
  ui._caption(text, x + h, y)
  return value, clicked
end
return ui
