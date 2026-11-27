LIB = asdl.lib.new("on1x.source.imports")
asdl.lib.version(LIB, "1")

-- The request is syntax, not an instruction to resolve or open a file.
function LIB.Check(edge)
    local spelling, alias, relative, quoted, selective, selectors = asdl.destruct(edge)
    local program = asdl.tag(edge) == "ProgramRequest"
    local function report(message)
        asdl.diagnostic.emit({severity = "error", location = asdl.location(edge), message = message})
    end
    local function ident(name)
        return name:match("^[a-zA-Z_][a-zA-Z0-9_]*$") ~= nil
    end
    if program then
        local valid = #spelling > 0 and #spelling <= 4096 and relative == 0
        if quoted then
            -- Keep the interior spelling opaque, including quotes and newlines.
            valid = valid and not spelling:find('[%z\\]')
        else
            valid = valid and not spelling:match("^%.") and not spelling:match("%.$")
                    and not spelling:find("..", 1, true)
            for component in spelling:gmatch("[^.]+") do
                valid = valid and ident(component)
            end
        end
        if not valid then
            report("invalid program import request")
            return edge
        end
    end
    if selective ~= (#selectors > 0) or (alias and (selective or not ident(alias))) then
        report("inconsistent import selector list or target alias")
        return edge
    end
    for _, selector in ipairs(selectors) do
        local name, rename, is_operator = asdl.destruct(selector)
        if (is_operator and (program or rename or name == "")) or
           (not is_operator and (not ident(name) or (rename and not ident(rename)))) then
            report("invalid import selector")
            break
        end
    end
    return edge
end

asdl.signlib(LIB)
return LIB
