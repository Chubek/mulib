local ui = require('immgtk.core')
function ui.label(text, x, y)
  ui._rect(x, y, 0, 0)
  imm_text(ui._string(text), x, y, ui.text)
end
return ui
