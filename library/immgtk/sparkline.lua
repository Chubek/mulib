local ui=require('immgtk.core')
function ui.sparkline(v,lo,hi,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);assert(#v>=1 and #v<=256);for i=1,#v do ui._range(v[i],lo,hi);local q=ui._clamp((v[i]-lo)/(hi-lo),0,1);imm_rect(x+(i-1)*w/#v,y+(1-q)*math.max(0,h-2),math.min(2,w/#v),math.min(2,h),ui.accent) end end
return ui
