local ui=require('immgtk.core')
function ui.spinner(phase,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);ui._number(phase);local k=math.floor((phase%1)*8);for i=0,7 do local t=i*math.pi/4;local s=math.max(1,math.min(w,h)/8);imm_rect(x+w/2+math.cos(t)*w*.3-s/2,y+h/2+math.sin(t)*h*.3-s/2,s,s,i==k and ui.accent or ui.normal) end end
return ui
