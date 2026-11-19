local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.timeline(id,current,start,finish,x,y,w,h) return a.slider(id,current,start,finish,x,y,w,h) end
function a.timeline_x(time,start,finish,x,w) assert(finish>start);return x+(time-start)/(finish-start)*w end
return a
