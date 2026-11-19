local ui=require('immgtk.core')
local a=require('immgtk.advanced')
require('immgtk.plotting.axes')
function a.plot_series(xs,ys,ax,ay,x,y,w,h,color) local px,py;local pushed=a.clip(x,y,w,h);for i=1,#ys do local u=a.plot_map(xs and xs[i] or i,ax[1],ax[2],ax[3]);local v=a.plot_map(ys[i],ay[1],ay[2],ay[3]);if u and v then local qx=x+u*w;local qy=y+(1-v)*h;if px then a.line(px,py,qx,qy,color or ui.accent) end;px,py=qx,qy else px,py=nil,nil end end;a.unclip(pushed) end
return a
