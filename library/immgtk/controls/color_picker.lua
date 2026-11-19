local ui = require('immgtk.core')
local a = require('immgtk.advanced')
function a.color_picker(id, rgba, x, y, w, h)
  a._argb(rgba); a._key(id)
  x, y, w, h = a._box(x, y, w, h, function(bx, by, bw, bh)
    for i = 1, 4 do ui._box(bx, by + (i - 1) * bh / 4, bw, bh / 4) end
  end)
  local result = rgba
  a.with_id(id, function()
    for i = 1, 4 do
      local base = 2 ^ ((4 - i) * 8)
      local old = math.floor(rgba / base) % 256
      local value = a.slider(tostring(i), old, 0, 255,
                             x, y + (i - 1) * h / 4, w, h / 4)
      result = result + (math.floor(value + .5) - old) * base
    end
  end)
  return result, result ~= rgba
end
return a
