local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.tab_bar(id,items,selected,x,y,w,h) a.push_id(id);local old=selected;for i=1,#items do local bx,by,bw,bh=a.row(x,y,w,h,#items,i);if a.button(tostring(i),items[i],bx,by,bw,bh) then selected=i end;if selected==i then imm_rect(bx,by+bh-2,bw,2,ui.accent) end end;a.pop_id();return selected,old~=selected end
return a
