local ui = require('immgtk.core')
-- Draw-only container: returns an inset content rectangle, without clipping.
function ui.group_box(text, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  assert(w >= 16 and h >= 32, 'group box dimensions')
  imm_rect(x, y, w, h, ui.hover)
  imm_rect(x + 1, y + 1, w - 2, h - 2, ui.background)
  ui._caption(text, x, y)
  return x + 8, y + 24, w - 16, h - 32
end
return ui
