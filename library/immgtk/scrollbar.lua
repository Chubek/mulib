local ui = require('immgtk.core')
-- Normalized vertical position; page is the visible fraction (0 < page <= 1).
function ui.scrollbar(id, value, page, x, y, w, h)
  ui._number(value); ui._number(page)
  assert(page > 0 and page <= 1, 'invalid scrollbar page')
  x, y, w, h = ui._box(x, y, w, h)
  local hot, held = imm_interact(id, x, y, w, h)
  local old = value
  local thumb = h * page
  if page == 1 then value = 0
  elseif held then
    local mx, my = imm_pointer()
    value = (my - y - thumb / 2) / (h - thumb)
  end
  value = ui._clamp(value, 0, 1)
  imm_rect(x, y, w, h, ui.background)
  imm_rect(x, y + (h - thumb) * value, w, thumb, ui._color(hot, held, true))
  return value, value ~= old
end
return ui
