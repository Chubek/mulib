local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.curve_editor(id,p) a.push_id(id);for i=1,4 do local q=p[i];local hot,held=a.hit(tostring(i),q.x-5,q.y-5,10,10);if held then q.x,q.y=imm_pointer() end;imm_rect(q.x-4,q.y-4,8,8,ui.accent) end;local px,py=p[1].x,p[1].y;for i=1,48 do local t=i/48;local u=1-t;local x=u^3*p[1].x+3*u*u*t*p[2].x+3*u*t*t*p[3].x+t^3*p[4].x;local y=u^3*p[1].y+3*u*u*t*p[2].y+3*u*t*t*p[3].y+t^3*p[4].y;a.line(px,py,x,y,ui.accent);px,py=x,y end;a.pop_id();return p end
return a
