local ui=require('immgtk.core')
function ui.histogram(v,lo,hi,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);assert(#v>=1 and #v<=256);for i=1,#v do ui._range(v[i],lo,hi);local q=ui._clamp((v[i]-lo)/(hi-lo),0,1)*h;imm_rect(x+(i-1)*w/#v,y+h-q,w/#v,q,ui.accent) end end
return ui
