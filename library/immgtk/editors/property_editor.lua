local ui=require('immgtk.core')
local a=require('immgtk.advanced')
require('immgtk.controls.input_text')
function a.property_editor(id,p,x,y,w,row) a.push_id(id);for i=1,#p do local q=p[i];ui._caption(q.name,x,y+(i-1)*row);if q.kind=='number' then q.value=a.slider(q.name,q.value,q.min,q.max,x+w/2,y+(i-1)*row,w/2,row) elseif q.kind=='bool' then if a.button(q.name,tostring(q.value),x+w/2,y+(i-1)*row,w/2,row) then q.value=not q.value end else q.value=a.input_text(q.name,q.value,x+w/2,y+(i-1)*row,w/2,row) end end;a.pop_id();return p end
return a
