local ui=require('immgtk.core')
function ui.color_swatch(id,c,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);assert(w>=4 and h>=4 and c>=0 and c<=0xffffffff and c==math.floor(c));local hot,held,clicked=imm_interact(id,x,y,w,h);imm_rect(x,y,w,h,ui._color(hot,held));imm_rect(x+2,y+2,w-4,h-4,c);return clicked end
return ui
