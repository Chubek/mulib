-- Load after the host binds imtui_native; no frame is needed to require this.
local native = assert(imtui_native, "host must bind the ImTUI Mune bridge")
local ui = {}

function ui.rect(x, y, w, h)
    return { x = x, y = y, w = w, h = h }
end

function ui.bounds()
    return ui.rect(native.bounds())
end

-- Tracks are one-based. A row is horizontal and a column is vertical.
function ui.row(rect, index, count, gap)
    return ui.rect(native.row(rect.x, rect.y, rect.w, rect.h, gap or 0, count, index))
end

function ui.column(rect, index, count, gap)
    return ui.rect(native.column(rect.x, rect.y, rect.w, rect.h, gap or 0, count, index))
end

function ui.text(x, y, text, style)
    if style == nil then
        native.text(x, y, text)
    else
        native.text(x, y, text, style.foreground, style.background, style.attributes)
    end
end

return ui
