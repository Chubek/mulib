local ui=require('immgtk.core')
local a=require('immgtk.advanced')
require('immgtk.controls.input_text')
function a.input_number(id,value,buffer,x,y,w,h) ui._number(value);if buffer~=nil then a._text(buffer) end;local nextbuf,changed=a.input_text(id,buffer or tostring(value),x,y,w,h);local n=tonumber(nextbuf);if n and n==n and math.abs(n)<math.huge then return n,nextbuf,n~=value end;return value,nextbuf,false end
return a
