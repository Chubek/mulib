local ui = require('immgtk.core')
-- Caller-supplied phase, in turns: deterministic and independent of wall time.
function ui.spinner(phase, x, y, w, h)
  ui._number(phase); x, y, w, h = ui._box(x, y, w, h)
  local active = math.floor((phase % 1) * 8)
  local size = math.min(w, h) / 5
  for i = 0, 7 do
    local angle = i * math.pi / 4
    imm_rect(x + w / 2 + math.cos(angle) * (w - size) / 2 - size / 2,
             y + h / 2 + math.sin(angle) * (h - size) / 2 - size / 2,
             size, size, i == active and ui.accent or ui.normal)
  end
end
return ui
