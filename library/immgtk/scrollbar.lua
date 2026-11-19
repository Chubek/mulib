local ui=require('immgtk.core')
function ui.scrollbar(id,v,page,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);ui._number(page);assert(page>0 and page<=1);local old=v;v=ui._clamp(v,0,1);local hot,held=imm_interact(id,x,y,w,h);local th=h*page;if page==1 then v=0 elseif held then local mx,my=imm_pointer();v=ui._clamp((my-y-th/2)/(h-th),0,1) end;imm_rect(x,y,w,h,ui.normal);imm_rect(x,y+(h-th)*v,w,th,ui._color(hot,held,true));return v,v~=old end
return ui
