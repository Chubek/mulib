local ui=require('immgtk.core')
function ui.heading(text,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);assert(h>=2);ui._caption(text,x,y);imm_rect(x,y+h-2,w,2,ui.accent) end
return ui
