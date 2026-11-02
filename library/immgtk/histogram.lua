local ui = require('immgtk.core')
function ui.histogram(values, lo, hi, x, y, w, h)
  ui._range(lo, lo, hi)
  assert(type(values) == 'table' and #values > 0 and #values <= 256, 'sample count')
  x, y, w, h = ui._box(x, y, w, h)
  for i = 1, #values do ui._number(values[i]) end
  imm_rect(x, y, w, h, ui.background)
  local bar = w / #values
  for i = 1, #values do
    local fill = h * ((ui._clamp(values[i], lo, hi) - lo) / (hi - lo))
    imm_rect(x + (i - 1) * bar, y + h - fill, bar, fill, ui.accent)
  end
end
return ui
