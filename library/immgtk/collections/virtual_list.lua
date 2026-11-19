local ui = require('immgtk.core')
local a = require('immgtk.advanced')
function a.virtual_list(count, offset, height, row)
  ui._number(count); ui._number(offset); ui._number(height); ui._number(row)
  assert(count == math.floor(count) and count >= 0 and count <= 1000000000,
         'invalid virtual row count')
  assert(offset >= 0 and offset <= 1000000000000 and
         height >= 0 and height <= 1000000 and row > 0 and row <= 1000000,
         'invalid virtual list geometry')
  local first = math.min(count + 1, math.floor(offset / row) + 1)
  if height == 0 then return first, first - 1 end
  local last = math.min(count, math.ceil((offset + height) / row))
  return first, math.max(first - 1, last)
end
return a
