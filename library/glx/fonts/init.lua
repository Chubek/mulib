-- Native PSF2 files are supplied by the application, not bundled.
local M={}
function M.candidates(home) local v={'/usr/share/fonts','/usr/local/share/fonts','/usr/share/kbd/consolefonts'};if home then v[#v+1]=home..'/.local/share/fonts' end;return v end
return M
