local M={acute={a='á',e='é',i='í',o='ó',u='ú',A='Á',E='É'},grave={a='à',e='è',i='ì',o='ò',u='ù'},tilde={n='ñ',N='Ñ',a='ã',o='õ'},diaeresis={a='ä',e='ë',i='ï',o='ö',u='ü'}}
function M.compose(mark,key) return M[mark] and M[mark][key] or nil end
return M
