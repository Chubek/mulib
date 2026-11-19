local ui=require('immgtk.core')
function ui.tooltip(text,visible,x,y,w,h) if not visible then return end;x,y,w,h=ui._box(x,y,w,h);imm_rect(x,y,w,h,ui.hover);ui._caption(text,x,y) end
return ui
