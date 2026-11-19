local ui=require('immgtk.core')
function ui.separator(x,y,w,h) x,y,w,h=ui._box(x,y,w,h);imm_rect(x,y+math.floor(h/2),w,1,ui.hover) end
return ui
