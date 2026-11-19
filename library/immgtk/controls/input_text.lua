local ui = require('immgtk.core')
local a = require('immgtk.advanced')
function a.input_text(id, value, x, y, w, h)
  local edited = a._edit_text(value)
  a._key(id)
  x, y, w, h = ui._box(x, y, w, h)
  local hot, held, clicked, focused = a.hit(id, x, y, w, h)
  local old = value
  if focused then value = edited end
  imm_rect(x, y, w, h, ui._color(hot, held, focused))
  ui._caption(value, x, y)
  if focused then a._consume_text() end
  return value, value ~= old
end
return a
