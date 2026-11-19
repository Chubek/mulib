local ui=require('immgtk.core')
function ui.slider(id,v,lo,hi,x,y,w,h) ui._range(v,lo,hi);x,y,w,h=ui._box(x,y,w,h);assert(w>=8 and h>=8);local old=v;local hot,held=imm_interact(id,x,y,w,h);if held then local mx=imm_pointer();v=lo+ui._clamp((mx-x)/w,0,1)*(hi-lo) end;v=ui._clamp(v,lo,hi);imm_rect(x,y,w,h,ui._color(hot,false));imm_rect(x,y,w*(v-lo)/(hi-lo),h,ui.accent);return v,v~=old end
return ui
