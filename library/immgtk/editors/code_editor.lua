local ui=require('immgtk.core')
local a=require('immgtk.advanced')
require('immgtk.controls.input_text')
function a.code_editor(id,text,x,y,w,h) return a.input_text(id,text,x,y,w,h) end
function a.code_tokens(s) local t={};for word in string.gmatch(s,'[%w_]+') do t[#t+1]={word=word,kind=tonumber(word) and 'number' or 'word'} end;return t end
return a
