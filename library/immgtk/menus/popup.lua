local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.popup(id,open,items,x,y,w,row) if not open then return false,nil end;a.push_id(id);row=row or 24;local choice;for i=1,#items do if a.button(tostring(i),items[i],x,y+(i-1)*row,w,row) then choice=i end end;a.pop_id();return not choice,choice end
return a
