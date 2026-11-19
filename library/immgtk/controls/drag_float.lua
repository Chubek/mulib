local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.drag_float(id,v,lo,hi,x,y,w,h) return a.slider(id,v,lo,hi,x,y,w,h) end
return a
