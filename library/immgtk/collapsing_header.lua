local ui = require('immgtk.core')
function ui.collapsing_header(id, text, open, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  open = not not open
  if clicked then open = not open end
  imm_rect(x, y, w, h, ui._color(hot, held))
  ui._caption((open and '- ' or '+ ') .. text, x, y)
  return open, clicked
end
return ui
