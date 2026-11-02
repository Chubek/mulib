local ui = require('immgtk.core')
function ui.radio(id, text, value, choice, x, y, w, h)
  ui._string(text); assert(choice ~= nil, 'radio choice required')
  x, y, w, h = ui._box(x, y, w, h)
  assert(w >= h and h >= 12, 'radio dimensions')
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  local old = value
  if clicked then value = choice end
  imm_rect(x, y, h, h, ui._color(hot, held))
  if value == choice then imm_rect(x + 4, y + 4, h - 8, h - 8, ui.accent) end
  ui._caption(text, x + h, y)
  return value, value ~= old
end
return ui
