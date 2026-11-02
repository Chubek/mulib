local ui = require('immgtk.core')
-- Bounded sample markers (not an antialiased line/path renderer).
function ui.sparkline(values, lo, hi, x, y, w, h)
  ui._range(lo, lo, hi)
  assert(type(values) == 'table' and #values > 0 and #values <= 256, 'sample count')
  x, y, w, h = ui._box(x, y, w, h)
  for i = 1, #values do ui._number(values[i]) end
  imm_rect(x, y, w, h, ui.background)
  local size = math.min(2, w / #values, h)
  for i = 1, #values do
    local t = (ui._clamp(values[i], lo, hi) - lo) / (hi - lo)
    local xx = x + (i - 1) * (w - size) / math.max(1, #values - 1)
    imm_rect(xx, y + (h - size) * (1 - t), size, size, ui.accent)
  end
end
return ui
