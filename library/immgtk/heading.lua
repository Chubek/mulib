local ui = require('immgtk.core')
-- Uses the application's font callback; no implicit font-size changes.
function ui.heading(text, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  assert(h >= 2, 'heading dimensions')
  ui._caption(text, x, y)
  imm_rect(x, y + h - 2, w, 2, ui.accent)
end
return ui
