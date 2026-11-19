local ui = require('immgtk.core')
local a = require('immgtk.advanced')
function a.range_slider(id, low, high, lo, hi, x, y, w, h)
  ui._range(low, lo, hi); ui._range(high, lo, hi); a._key(id)
  x, y, w, h = a._box(x, y, w, h, function(bx, by, bw, bh)
    -- Check both boxes before submitting either control or consuming layout.
    ui._box(bx, by, bw, bh / 2); ui._box(bx, by + bh / 2, bw, bh / 2)
  end)
  local u, v
  a.with_id(id, function()
    u = a.slider('low', low, lo, hi, x, y, w, h / 2)
    v = a.slider('high', high, lo, hi, x, y + h / 2, w, h / 2)
  end)
  if u > v then
    if u ~= low then u = v else v = u end
  end
  return u, v, u ~= low or v ~= high
end
return a
