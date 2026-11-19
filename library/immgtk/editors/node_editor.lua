local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.node_editor(id,nodes,edges) a.push_id(id);local lookup={};for i=1,#nodes do lookup[nodes[i].id]=nodes[i] end;for i=1,#edges do local e=edges[i];local n,m=lookup[e[1]],lookup[e[2]];if n and m then a.line(n.x+n.w,n.y+n.h/2,m.x,m.y+m.h/2,ui.accent) end end;local selected;for i=1,#nodes do local n=nodes[i];local hot,held,click=a.hit(tostring(n.id),n.x,n.y,n.w,n.h);if held and a.input.dx then n.x=n.x+a.input.dx;n.y=n.y+(a.input.dy or 0) end;imm_rect(n.x,n.y,n.w,n.h,ui._color(hot,held));ui._caption(n.label,n.x,n.y);if click then selected=n.id end end;a.pop_id();return selected end
return a
