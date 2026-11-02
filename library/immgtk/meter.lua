local ui = require('immgtk.core')
function ui.meter(value, lo, hi, segments, x, y, w, h)
  ui._range(value, lo, hi); ui._index(segments, 128)
  x, y, w, h = ui._box(x, y, w, h)
  assert(w >= 2 * segments, 'meter dimensions')
  local filled = math.floor((ui._clamp(value, lo, hi) - lo) / (hi - lo) * segments)
  local width = w / segments
  for i = 1, segments do
    imm_rect(x + (i - 1) * width, y, width - 1, h,
             i <= filled and ui.accent or ui.normal)
  end
end
return ui
