local ui = require('immgtk.core')
function ui.badge(text, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  imm_rect(x, y, w, h, ui.accent)
  ui._caption(text, x, y)
end
return ui
