local ui = require('immgtk.core')
local a = require('immgtk.advanced')
function a.scroll_area(id, offset, content, x, y, w, h, draw)
  a._key(id); ui._number(offset); ui._number(content)
  assert(content >= 0 and content <= 1000000000000 and
         math.abs(offset) <= 1000000000000, 'invalid scroll extent')
  assert(type(draw) == 'function', 'scroll callback required')
  x, y, w, h = ui._box(x, y, w, h)
  local mx, my = imm_pointer()
  ui._number(mx); ui._number(my)
  if mx >= x and my >= y and mx < x + w and my < y + h then
    offset = offset - (a.input.scroll or 0) * 24
  end
  offset = ui._clamp(offset, 0, math.max(0, content - h))
  a.with_id(id, function()
    a.with_clip(x, y, w, h, function()
      draw(x, y - offset, w, h, offset)
    end)
  end)
  return offset
end
return a
