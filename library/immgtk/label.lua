local ui=require('immgtk.core')
function ui.label(text,x,y) imm_text(ui._string(text),ui._number(x),ui._number(y),ui.text) end
return ui
