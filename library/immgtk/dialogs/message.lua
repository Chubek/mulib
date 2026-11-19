local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.message(id,text,x,y,w,h) a.push_id(id);imm_rect(x,y,w,h,ui.normal);ui._caption(text,x,y);local ok=a.button('ok','OK',x+w-70,y+h-30,60,24);a.pop_id();return ok end
return a
