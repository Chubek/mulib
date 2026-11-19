local ui=require('immgtk.core')
function ui.progress(v,lo,hi,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);ui._range(v,lo,hi);imm_rect(x,y,w,h,ui.normal);imm_rect(x,y,w*ui._clamp((v-lo)/(hi-lo),0,1),h,ui.accent) end
return ui
