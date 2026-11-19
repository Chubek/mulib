local ui=require('immgtk.core')
function ui.stepper(id,v,step,lo,hi,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);ui._range(v,lo,hi);assert(step>0);local old=v;local hot,held,c=imm_interact(id,x,y,w,h);if c then local mx=imm_pointer();v=v+(mx<x+w/2 and -step or step) end;v=ui._clamp(v,lo,hi);imm_rect(x,y,w,h,ui._color(hot,held));ui._caption('-  '..tostring(v)..'  +',x,y);return v,v~=old end
return ui
