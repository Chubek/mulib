local ui = require('immgtk.core')
function ui.progress(value, lo, hi, x, y, w, h)
  ui._range(value, lo, hi); x, y, w, h = ui._box(x, y, w, h)
  imm_rect(x, y, w, h, ui.normal)
  imm_rect(x, y, w * ((ui._clamp(value, lo, hi) - lo) / (hi - lo)), h, ui.accent)
end
return ui
