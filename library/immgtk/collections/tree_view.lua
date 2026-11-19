local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.tree_view(id,nodes,x,y,w,row) row=row or 24;a.push_id(id);local pos=y;local function visit(list,depth) for i=1,#list do local n=list[i];a.push_id(n.id or tostring(i));local label=(n.children and (n.open and '- ' or '+ ') or '  ')..n.label;if a.button('row',label,x+depth*12,pos,w-depth*12,row) then n.open=not n.open end;pos=pos+row;if n.open and n.children and depth<32 and w>(depth+1)*12 then visit(n.children,depth+1) end;a.pop_id() end end;visit(nodes,0);a.pop_id();return pos-y end
return a
