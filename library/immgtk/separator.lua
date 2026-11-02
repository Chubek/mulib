local ui = require('immgtk.core')
function ui.separator(x, y, w, h)
  x, y, w, h = ui._box(x, y, w, h)
  imm_rect(x, y + h / 2, w, math.min(1, h), ui.hover)
end
return ui
