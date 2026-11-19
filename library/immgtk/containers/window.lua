local ui = require('immgtk.core')
local a = require('immgtk.advanced')
local function bounds(x, y, w, h)
  ui._box(x, y, w, h)
  assert(w > 16 and h > 24, 'window too small')
  ui._box(x + 8, y + 32, w - 16, 24)
  ui._rect(x, y + 24, w, h - 24)
end
function a.begin_window(id, title, rect)
  a._key(id); a._text(title)
  assert(type(rect) == 'table' and getmetatable(rect) == nil,
         'plain window bounds required')
  local x, y, w, h = rect.x, rect.y, rect.w, rect.h
  bounds(x, y, w, h)
  a._open('window', id, function()
    local hot, held = a.hit('title', x, y, w, 24)
    if held then
      x, y = x + (a.input.dx or 0), y + (a.input.dy or 0)
      bounds(x, y, w, h)
    end
    imm_rect(x, y, w, h, ui.background)
    imm_rect(x, y, w, 24, ui.active)
    ui._caption(title, x, y)
    local pushed = a.clip(x, y + 24, w, h - 24)
    ui.layout(x + 8, y + 32, w - 16)
    return pushed
  end)
  rect.x, rect.y = x, y
  return rect
end
function a.end_window() a._close('window') end
return a
