local ui=require('immgtk.core')
function ui.combo_box(id,items,selected,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);ui._items(items);ui._index(selected,#items);local old=selected;local hot,held,c=imm_interact(id,x,y,w,h);if c then selected=selected%#items+1 end;imm_rect(x,y,w,h,ui._color(hot,held));ui._caption(items[selected],x,y);return selected,selected~=old end
return ui
