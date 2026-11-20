local ui = require("imtui.core")
local native = imtui_native

function ui.button(key, rect, label, enabled)
    return native.button(key, rect.x, rect.y, rect.w, rect.h, label, enabled)
end

function ui.checkbox(key, rect, label, value, enabled)
    return native.checkbox(key, rect.x, rect.y, rect.w, rect.h, label, value, enabled)
end

-- A one-row horizontal separator. No drawing beyond rect or the text limit.
function ui.rule(rect, style)
    if rect.w > 0 and rect.h > 0 then
        ui.text(rect.x, rect.y, string.rep("-", math.min(rect.w, 4096)), style)
    end
end

-- A button that cycles through choices; the caller owns the selected index.
function ui.choice(key, rect, labels, selected, enabled)
    local n = #labels
    assert(math.type(selected) == "integer" and selected >= 1 and selected <= n,
           "invalid ImTUI choice index")
    if ui.button(key, rect, labels[selected], enabled) then
        return selected % n + 1, true
    end
    return selected, false
end

return ui
