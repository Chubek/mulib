-- Adapt the real QMR compiler's text-only Lua output to Mune. There is no
-- include resolver: a manifest can only consume its explicit data model.
local function quote(text)
    local escaped = text:gsub('[%z\1-\31\\"]', function(c)
        if c == '\\' then return '\\\\' end
        if c == '"' then return '\\"' end
        return string.format('\\u%04x', string.byte(c))
    end)
    return '"' .. escaped .. '"'
end

function __qmr_emit_native(value, escape)
    local kind = type(value)
    if kind == "string" then
        return __qmr_write_native(escape and quote(value) or value)
    end
    if kind == "nil" then error("QMR field is missing") end
    if kind == "number" or kind == "boolean" then
        return __qmr_write_native(tostring(value))
    end
    error("QMR value must be a string, number, or boolean")
end

function __qmr_include_native()
    error("QMR includes are not available in On1xfile")
end
