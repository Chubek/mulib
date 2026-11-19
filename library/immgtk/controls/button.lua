local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.control_button(id,label,x,y,w,h) return a.button(id,label,x,y,w,h) end
return a
