local ui = require('immgtk.core')
function ui.selectable(id, text, selected, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  selected = not not selected
  if clicked then selected = not selected end
  imm_rect(x, y, w, h, ui._color(hot, held, selected))
  ui._caption(text, x, y)
  return selected, clicked
end
return ui
