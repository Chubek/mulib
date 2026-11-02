local ui = require('immgtk.core')
-- Nearest-neighbor ARGB pixel grid using only the host rectangle primitive.
function ui.image(pixels, columns, rows, x, y, w, h)
  ui._index(columns, 256); ui._index(rows, 256)
  assert(type(pixels) == 'table' and #pixels == columns * rows and
         #pixels <= 4096, 'image pixel count')
  x, y, w, h = ui._box(x, y, w, h)
  for i = 1, #pixels do
    local color = ui._number(pixels[i])
    assert(color >= 0 and color <= 0xffffffff and color == math.floor(color),
           'invalid image color')
  end
  for row = 0, rows - 1 do
    for column = 0, columns - 1 do
      imm_rect(x + column * w / columns, y + row * h / rows,
               w / columns, h / rows, pixels[row * columns + column + 1])
    end
  end
end
return ui
