local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.menu_bar(id,items,x,y,w,h) a.push_id(id);local result;for i=1,#items do if a.button(tostring(i),items[i],a.row(x,y,w,h,#items,i)) then result=i end end;a.pop_id();return result end
return a
