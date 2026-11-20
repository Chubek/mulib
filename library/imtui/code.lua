local ui = require("imtui.core")
local native = imtui_native

-- Read-only viewport; the caller passes a one-based first line for scrolling.
function ui.code(rect, language, source, first_line)
    native.code(rect.x, rect.y, rect.w, rect.h, language, source, first_line)
end

return ui
