local ui=require('immgtk.core')
function ui.selectable(id,text,v,x,y,w,h) ui._string(text);x,y,w,h=ui._box(x,y,w,h);local hot,held,c=imm_interact(id,x,y,w,h);if c then v=not v end;imm_rect(x,y,w,h,ui._color(hot,held,v));ui._caption(text,x,y);return v,c end
return ui
