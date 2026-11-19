local ui=require('immgtk.core')
local a=require('immgtk.advanced')
require('immgtk.containers.splitter')
function a.dock_arrange(node,x,y,w,h,out) out=out or {};if node.window then out[node.window]={x=x,y=y,w=w,h=h};return out end;local p=a.splitter(node.id,{x=x,y=y,w=w,h=h},node.ratio or .5,node.vertical);node.ratio=p;if node.vertical then a.dock_arrange(node.left,x,y,w,h*p,out);a.dock_arrange(node.right,x,y+h*p,w,h*(1-p),out) else a.dock_arrange(node.left,x,y,w*p,h,out);a.dock_arrange(node.right,x+w*p,y,w*(1-p),h,out) end;return out end
return a
