local ui=require('immgtk.core')
function ui.meter(v,lo,hi,n,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);ui._range(v,lo,hi);assert(n>=1 and n<=128 and n==math.floor(n) and w>=2*n);local q=math.floor(n*ui._clamp((v-lo)/(hi-lo),0,1));for i=1,n do imm_rect(x+(i-1)*w/n,y,w/n-1,h,i<=q and ui.accent or ui.normal) end end
return ui
