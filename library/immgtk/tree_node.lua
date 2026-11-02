local ui = require('immgtk.core')
function ui.tree_node(id, text, open, depth, x, y, w, h)
  ui._string(text); ui._number(depth)
  assert(depth >= 0 and depth <= 64 and depth == math.floor(depth), 'tree depth')
  x, y, w, h = ui._box(x, y, w, h)
  local indent = depth * 12
  assert(w > indent, 'tree node dimensions')
  x = x + indent; w = w - indent
  local hot, held, clicked = imm_interact(id, x, y, w, h)
  open = not not open
  if clicked then open = not open end
  imm_rect(x, y, w, h, ui._color(hot, held))
  ui._caption((open and '- ' or '+ ') .. text, x, y)
  return open, clicked
end
return ui
