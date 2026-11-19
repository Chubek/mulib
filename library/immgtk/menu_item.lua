local ui=require('immgtk.core')
function ui.menu_item(id,text,enabled,x,y,w,h) ui._string(text);x,y,w,h=ui._box(x,y,w,h);if not enabled then imm_rect(x,y,w,h,ui.normal);ui._caption(text,x,y);return false end;local hot,held,c=imm_interact(id,x,y,w,h);imm_rect(x,y,w,h,ui._color(hot,held));ui._caption(text,x,y);return c end
return ui
