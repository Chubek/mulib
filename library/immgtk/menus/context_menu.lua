local ui=require('immgtk.core')
local a=require('immgtk.advanced')
require('immgtk.menus.popup')
function a.context_menu(id,open,items,x,y,w,row) if a.input.context_menu then open=true end;return a.popup(id,open,items,x,y,w,row) end
return a
