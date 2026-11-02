local ui = require('immgtk.core')
function ui.color_swatch(id, color, x, y, w, h)
  ui._number(color)
  assert(color >= 0 and color <= 0xffffffff and color == math.floor(color),
         'invalid swatch color')
  x, y, w, h = ui._box(x, y, w, h)
  assert(w >= 4 and h >= 4, 'swatch dimensions')
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  imm_rect(x, y, w, h, ui._color(hot, held))
  imm_rect(x + 2, y + 2, w - 4, h - 4, color)
  return clicked
end
return ui
