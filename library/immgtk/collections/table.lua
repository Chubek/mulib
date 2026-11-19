local ui=require('immgtk.core')
local a=require('immgtk.advanced')
require('immgtk.collections.virtual_list')
function a.table_view(id,model,columns,offset,x,y,w,h,row) row=row or 24;if columns<1 then return end;a.push_id(id);local first,last=a.virtual_list(#model,offset,h,row);local p=a.clip(x,y,w,h);for i=first,last do for j=1,columns do local bx,by,bw,bh=a.row(x,y+(i-1)*row-offset,w,row,columns,j);imm_rect(bx,by,bw-1,bh-1,ui.normal);ui._caption(tostring(model[i][j] or ''),bx,by) end end;a.unclip(p);a.pop_id() end
return a
