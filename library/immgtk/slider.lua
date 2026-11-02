local ui = require('immgtk.core')
function ui.slider(id, value, lo, hi, x, y, w, h)
  ui._range(value, lo, hi); x, y, w, h = ui._box(x, y, w, h)
  assert(w >= 8 and h >= 8, 'slider dimensions')
  local hot, held = imm_interact(id, x, y, w, h)
  local old = value
  if held then
    local mx = imm_pointer()
    value = lo + ui._clamp((mx - x) / w, 0, 1) * (hi - lo)
  end
  value = ui._clamp(value, lo, hi)
  imm_rect(x, y, w, h, ui._color(hot, false))
  imm_rect(x, y, w * ((value - lo) / (hi - lo)), h, ui.accent)
  return value, value ~= old
end
return ui
