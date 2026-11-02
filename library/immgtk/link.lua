local ui = require('immgtk.core')
-- Returns activation; never launches a browser or executes a command.
function ui.link(id, text, x, y, w, h)
  ui._string(text); x, y, w, h = ui._box(x, y, w, h)
  assert(h >= 2, 'link dimensions')
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  imm_text(text, x, y, held and ui.text or ui.accent)
  if hot then imm_rect(x, y + h - 1, w, 1, ui.accent) end
  return clicked
end
return ui
