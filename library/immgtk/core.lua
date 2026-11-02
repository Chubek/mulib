-- Shared theme, validation and layout only; no widgets or implicit globals.
local ui = {
  normal = 0xff343b48, hover = 0xff46556a, active = 0xff2465a8,
  text = 0xffeeeeee, background = 0xff202630, accent = 0xff56b4e9
}

function ui._number(value)
  assert(type(value) == 'number' and value == value and
         value > -math.huge and value < math.huge, 'finite number required')
  return value
end

function ui._rect(x, y, w, h)
  ui._number(x); ui._number(y); ui._number(w); ui._number(h)
  assert(math.abs(x) <= 1000000 and math.abs(y) <= 1000000 and
         w >= 0 and h >= 0 and w <= 1000000 and h <= 1000000,
         'invalid widget geometry')
  return x, y, w, h
end

function ui._box(x, y, w, h)
  if x == nil then x, y, w, h = ui.next() end
  ui._rect(x, y, w, h)
  assert(w > 0 and h > 0, 'positive widget size required')
  return x, y, w, h
end

function ui._string(text)
  assert(type(text) == 'string', 'text must be a string')
  return text
end

function ui._clamp(value, lo, hi)
  return math.max(lo, math.min(hi, value))
end

function ui._range(value, lo, hi)
  ui._number(value); ui._number(lo); ui._number(hi); ui._number(hi - lo)
  assert(hi > lo and hi - lo > 0, 'increasing, representable range required')
end

function ui._color(hot, held, selected)
  return held and ui.active or (hot and ui.hover or
         (selected and ui.accent or ui.normal))
end

function ui._caption(text, x, y)
  imm_text(ui._string(text), x + 4, y + 4, ui.text)
end

function ui._items(items)
  assert(type(items) == 'table' and #items > 0 and #items <= 256,
         '1..256 items required')
  for i = 1, #items do ui._string(items[i]) end
end

function ui._index(value, count)
  ui._number(value)
  assert(value == math.floor(value) and value >= 1 and value <= count,
         'selection out of range')
end

function ui.layout(x, y, w, row, gap)
  row = row or 24; gap = gap or 4
  ui._box(x, y, w, row); ui._number(gap)
  assert(gap >= 0 and gap <= 1000000, 'invalid layout gap')
  ui.lx = x; ui.ly = y; ui.lw = w; ui.lr = row; ui.lg = gap
end

function ui.next()
  assert(ui.lx ~= nil, 'call ui.layout first')
  local x, y, w, h = ui._rect(ui.lx, ui.ly, ui.lw, ui.lr)
  ui.ly = y + h + ui.lg
  return x, y, w, h
end

return ui
