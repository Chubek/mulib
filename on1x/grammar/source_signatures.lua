LIB = asdl.lib.new("on1x.source.signatures")
asdl.lib.version(LIB, "1")

-- The compiler has already checked type equality, visibility and ownership.
-- Here we check the typed declaration projection, not implementation types.
function LIB.Check(signature)
    local name, members = asdl.destruct(signature)
    if name == "" or #members == 0 then
        asdl.diagnostic.emit({severity = "error", location = asdl.location(signature),
                              message = "empty verified module signature projection"})
        return signature
    end
    local types, values = {}, {}
    for _, member in ipairs(members) do
        local spelling = asdl.destruct(member)
        local namespace = asdl.tag(member) == "AbstractType" and types or values
        if spelling == "" or namespace[spelling] then
            asdl.diagnostic.emit({severity = "error", location = asdl.location(member),
                                  message = "invalid verified signature member projection"})
            return signature
        end
        namespace[spelling] = true
    end
    return signature
end

asdl.signlib(LIB)
return LIB
