local ui=require('immgtk.core')
local a=require('immgtk.advanced')
require('immgtk.collections.list_view')
function a.file_dialog(id,entries,selected,offset,x,y,w,h) return a.list_view(id,entries,selected,offset,x,y,w,h,24) end
return a
