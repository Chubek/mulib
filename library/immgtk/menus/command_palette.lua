local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.command_palette(id,items,query,x,y,w,row) a.push_id(id);local out;local k=0;query=string.lower(query or '');for i=1,#items do if string.find(string.lower(items[i]),query,1,true) then if a.button(tostring(i),items[i],x,y+k*row,w,row) then out=i end;k=k+1;if k==20 then break end end end;a.pop_id();return out end
return a
