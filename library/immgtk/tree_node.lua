local ui=require('immgtk.core')
function ui.tree_node(id,text,open,depth,x,y,w,h) x,y,w,h=ui._box(x,y,w,h);assert(depth>=0 and depth<=64 and depth==math.floor(depth) and w>depth*12);x=x+depth*12;w=w-depth*12;local hot,held,c=imm_interact(id,x,y,w,h);if c then open=not open end;imm_rect(x,y,w,h,ui._color(hot,held));ui._caption((open and '- ' or '+ ')..text,x,y);return open,c end
return ui
