local ui = require('immgtk.core')
function ui.panel(x, y, w, h)
  ui._rect(x, y, w, h)
  imm_rect(x, y, w, h, ui.background)
end
return ui
