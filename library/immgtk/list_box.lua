local ui=require('immgtk.core')
function ui.list_box(id,items,selected,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);ui._items(items);ui._index(selected,#items);assert(h>=8*#items);local old=selected;local hot,held,c=imm_interact(id,x,y,w,h);local rh=h/#items;if c then local mx,my=imm_pointer();selected=ui._clamp(math.floor((my-y)/rh)+1,1,#items) end;for i=1,#items do imm_rect(x,y+(i-1)*rh,w,rh,i==selected and ui.accent or ui.normal);ui._caption(items[i],x,y+(i-1)*rh) end;return selected,selected~=old end
return ui
