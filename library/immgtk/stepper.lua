local ui = require('immgtk.core')
-- Left half decrements, right half increments. Activation uses the same split.
function ui.stepper(id, value, step, lo, hi, x, y, w, h)
  ui._range(value, lo, hi); ui._number(step)
  assert(step > 0, 'positive step required')
  x, y, w, h = ui._box(x, y, w, h)
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  local old = value
  value = ui._clamp(value, lo, hi)
  if clicked then
    local mx = imm_pointer()
    if mx < x + w / 2 then
      value = step >= value - lo and lo or value - step
    else
      value = step >= hi - value and hi or value + step
    end
  end
  imm_rect(x, y, w, h, ui._color(hot, held))
  ui._caption('-  ' .. tostring(value) .. '  +', x, y)
  return value, value ~= old
end
return ui
