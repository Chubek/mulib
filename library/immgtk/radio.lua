local ui=require('immgtk.core')
function ui.radio(id,text,v,choice,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);assert(choice~=nil and w>=h and h>=12);local hot,held,c=imm_interact(id,x,y,w,h);local old=v;if c then v=choice end;imm_rect(x,y,h,h,ui._color(hot,held));if v==choice then imm_rect(x+3,y+3,h-6,h-6,ui.accent) end;ui._caption(text,x+h,y);return v,v~=old end
return ui
