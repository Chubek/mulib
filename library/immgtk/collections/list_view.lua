local ui = require('immgtk.core')
local a = require('immgtk.advanced')
require('immgtk.collections.virtual_list')
function a.list_view(id, items, selected, offset, x, y, w, h, row)
  row = row or 24
  a._key(id)
  assert(type(items) == 'table' and #items <= 65536, 'list item limit exceeded')
  for i = 1, #items do a._text(items[i]) end
  if selected ~= nil and selected ~= 0 then ui._index(selected, #items) end
  local first, last
  x, y, w, h = a._box(x, y, w, h, function(bx, by, bw, bh)
    first, last = a.virtual_list(#items, offset, bh, row)
    assert(last - first < 4096, 'too many visible rows')
    for i = first, last do ui._box(bx, by + (i - 1) * row - offset, bw, row) end
  end)
  local old = selected
  a.with_id(id, function()
    a.with_clip(x, y, w, h, function()
      for i = first, last do
        local by = y + (i - 1) * row - offset
        if a.button(tostring(i), items[i], x, by, w, row) then selected = i end
        if i == selected then imm_rect(x, by, math.min(3, w), row, ui.accent) end
      end
    end)
  end)
  return selected, old ~= selected
end
return a
