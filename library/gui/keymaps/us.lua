local M={normal={},shift={}}
local base={2,16,30,44};local a={'1234567890-=','qwertyuiop[]',"asdfghjkl;'`",'zxcvbnm,./'};local b={'!@#$%^&*()_+','QWERTYUIOP{}','ASDFGHJKL:"~','ZXCVBNM<>?'}
for i=1,#a do for j=1,#a[i] do M.normal[base[i]+j-1]=string.sub(a[i],j,j);M.shift[base[i]+j-1]=string.sub(b[i],j,j) end end;M.normal[57]=' ';M.shift[57]=' '
function M.translate(code,shift,caps) local v=M.normal[code];if v and string.match(v,'[a-z]') and caps then shift=not shift end;return (shift and M.shift[code] or v) end
return M
