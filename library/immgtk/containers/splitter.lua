local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.splitter(id,r,ratio,vertical) local x,y,w,h=r.x,r.y,r.w,r.h;ratio=ui._clamp(ratio,.05,.95);local bar=vertical and y+h*ratio or x+w*ratio;local hot,held;if vertical then hot,held=a.hit(id,x,bar-3,w,6) else hot,held=a.hit(id,bar-3,y,6,h) end;if held then local mx,my=imm_pointer();ratio=ui._clamp(vertical and (my-y)/h or (mx-x)/w,.05,.95) end;if vertical then imm_rect(x,y+h*ratio-2,w,4,ui._color(hot,held)) else imm_rect(x+w*ratio-2,y,4,h,ui._color(hot,held)) end;return ratio end
return a
