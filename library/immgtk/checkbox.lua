local ui=require('immgtk.core')
function ui.checkbox(id,text,v,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);assert(w>=h and h>=12);local hot,held,c=imm_interact(id,x,y,w,h);if c then v=not v end;imm_rect(x,y,h,h,ui._color(hot,held));if v then imm_rect(x+3,y+3,h-6,h-6,ui.accent) end;ui._caption(text,x+h,y);return v,c end
return ui
