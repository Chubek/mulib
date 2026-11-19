local ui = require('immgtk.core')
local a = require('immgtk.advanced')
function a.drag_vector(id, values, lo, hi, x, y, w, h)
  assert(type(values) == 'table' and getmetatable(values) == nil and
         #values >= 1 and #values <= 16, '1..16 plain vector values required')
  for i = 1, #values do ui._range(values[i], lo, hi) end
  a._key(id)
  x, y, w, h = a._box(x, y, w, h, function(bx, by, bw, bh)
    assert(bw >= #values, 'vector columns too narrow')
    for i = 1, #values do ui._box(a.row(bx, by, bw, bh, #values, i)) end
  end)
  local pending, changed = {}, false
  a.with_id(id, function()
    for i = 1, #values do
      local value, c = a.slider(i, values[i], lo, hi,
                               a.row(x, y, w, h, #values, i))
      pending[i], changed = value, changed or c
    end
  end)
  -- Native failure on a later component must not partially mutate the model.
  for i = 1, #values do values[i] = pending[i] end
  return values, changed
end
return a
