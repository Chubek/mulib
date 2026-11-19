local M={}
function M.check(x,y,w,h) return {{x+w*.15,y+h*.5,x+w*.4,y+h*.75},{x+w*.4,y+h*.75,x+w*.85,y+h*.2}} end
function M.close(x,y,w,h) return {{x+w*.2,y+h*.2,x+w*.8,y+h*.8},{x+w*.8,y+h*.2,x+w*.2,y+h*.8}} end
function M.plus(x,y,w,h) return {{x+w*.2,y+h*.5,x+w*.8,y+h*.5},{x+w*.5,y+h*.2,x+w*.5,y+h*.8}} end
return M
