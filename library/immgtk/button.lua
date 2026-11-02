local ui = require('immgtk.core')
function ui.button(id, text, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  imm_rect(x, y, w, h, ui._color(hot, held))
  ui._caption(text, x, y)
  return clicked
end
return ui
