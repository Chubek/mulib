local ui=require('immgtk.core')
local a=require('immgtk.advanced')
function a.heatmap(v,rows,cols,lo,hi,x,y,w,h) assert(rows>0 and cols>0 and hi>lo);for j=1,rows do for i=1,cols do local t=ui._clamp((v[(j-1)*cols+i]-lo)/(hi-lo),0,1);local c=0xff000000+math.floor(t*255)*65536+math.floor((1-t)*255);imm_rect(x+(i-1)*w/cols,y+(j-1)*h/rows,w/cols,h/rows,c) end end end
return a
