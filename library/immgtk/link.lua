local ui=require('immgtk.core')
function ui.link(id,text,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);assert(h>=2);local hot,held,c=imm_interact(id,x,y,w,h);imm_text(text,x+4,y+4,ui.accent);if hot then imm_rect(x,y+h-2,w,1,ui.accent) end;return c end
return ui
