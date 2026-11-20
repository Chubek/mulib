local ui = require("imtui")

local source = [[
/* A read-only preview */
int square(int n) {
    return n * n;
}
]]

-- require("imtui.demo") returns a function to retain and call each frame.
local shown, first = true, 1
return function()
    local b = ui.bounds()
    local controls = ui.rect(b.x, b.y, b.w, math.min(b.h, 1))
    shown = ui.checkbox("demo/preview", ui.row(controls, 1, 2, 1), "Preview", shown)
    if ui.button("demo/next", ui.row(controls, 2, 2, 1), "Next line", shown) then
        first = first % 4 + 1
    end
    if b.h > 1 then
        ui.rule(ui.rect(b.x, b.y + 1, b.w, 1))
    end
    if shown and b.h > 2 then
        ui.code(ui.rect(b.x, b.y + 2, b.w, b.h - 2), "c", source, first)
    end
end
