local ui=require('immgtk.core')
function ui.toggle(id,v,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);assert(w>=2*h and h>=8);local hot,held,c=imm_interact(id,x,y,w,h);if c then v=not v end;imm_rect(x,y,w,h,ui._color(hot,held,v));imm_rect(v and x+w-h+2 or x+2,y+2,h-4,h-4,ui.text);return v,c end
return ui
