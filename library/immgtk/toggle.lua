local ui = require('immgtk.core')
function ui.toggle(id, value, x, y, w, h)
  x, y, w, h = ui._box(x, y, w, h)
  assert(w >= 2 * h and h >= 8, 'toggle dimensions')
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  value = not not value
  if clicked then value = not value end
  imm_rect(x, y, w, h, ui._color(hot, held, value))
  imm_rect(value and x + w - h + 2 or x + 2, y + 2, h - 4, h - 4, ui.text)
  return value, clicked
end
return ui
