local ui = require('immgtk.core')
function ui.menu_item(id, text, enabled, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  local hot, held, clicked = false, false, false
  if enabled then hot, held, clicked = imm_interact(id, x, y, w, h) end
  imm_rect(x, y, w, h, ui._color(hot, held))
  imm_text(text, x + 4, y + 4, enabled and ui.text or ui.hover)
  return clicked
end
return ui
