local ui = require('immgtk.core')
function ui.vslider(id, value, lo, hi, x, y, w, h)
  ui._range(value, lo, hi); x, y, w, h = ui._box(x, y, w, h)
  assert(w >= 8 and h >= 8, 'vslider dimensions')
  local hot, held = imm_interact(id, x, y, w, h)
  local old = value
  if held then
    local mx, my = imm_pointer()
    value = lo + ui._clamp(1 - (my - y) / h, 0, 1) * (hi - lo)
  end
  value = ui._clamp(value, lo, hi)
  local fill = h * ((value - lo) / (hi - lo))
  imm_rect(x, y, w, h, ui._color(hot, false))
  imm_rect(x, y + h - fill, w, fill, ui.accent)
  return value, value ~= old
end
return ui
