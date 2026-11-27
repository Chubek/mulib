LIB = asdl.lib.new("on1x.source.policy")
asdl.lib.version(LIB, "1")

function LIB.Header(header)
    local name = asdl.destruct(header)
    local depth = 0
    local valid = name ~= "" and not name:match("^%.") and not name:match("%.$")
                  and not name:find("..", 1, true)
    if valid then
        for part in name:gmatch("[^.]+") do
            if not part:match("^[a-zA-Z_][a-zA-Z0-9_]*$") then
                valid = false
                break
            end
            depth = depth + 1
        end
    end
    if not valid then
        asdl.diagnostic.emit({severity = "error", location = asdl.location(header),
                              message = "invalid hierarchical module declaration"})
        return nil
    end
    return depth
end

function LIB.Declaration(declaration, depth)
    local _, _, visibility = asdl.destruct(declaration)
    if asdl.tag(visibility) == "SourceShared" then
        local spelling = asdl.destruct(visibility)
        local elevation = spelling:match("^%d+$") and tonumber(spelling)
        if not elevation or elevation < 1 or elevation > depth then
            asdl.diagnostic.emit({severity = "error", location = asdl.location(declaration),
                                  code = "ON1X-MODULE-ELEVATION",
                                  message = "shared elevation must be within the declaring module depth"})
        end
    end
    return declaration
end

asdl.signlib(LIB)
return LIB
