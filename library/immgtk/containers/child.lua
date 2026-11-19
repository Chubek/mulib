local ui = require('immgtk.core')
local a = require('immgtk.advanced')
function a.begin_child(id, x, y, w, h)
  assert(x ~= nil, 'explicit child geometry required')
  a._key(id); ui._box(x, y, w, h)
  return a._open('child', id, function()
    local pushed = a.clip(x, y, w, h)
    ui.layout(x, y, w)
    return pushed
  end)
end
function a.end_child(state)
  assert(type(state) == 'table', 'child state required')
  a._close('child', state)
end
return a
