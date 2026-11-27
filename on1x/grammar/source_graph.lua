LIB = asdl.lib.new("on1x.source.graph")
asdl.lib.version(LIB, "1")

-- IDs are one-based indices into the loader's physical dependency sequence.
-- Neither a name, synthetic group, nor a Lua filesystem probe supplies an ID.
function LIB.Check(graph)
    local paths, edges = asdl.destruct(graph)
    local function report(message)
        asdl.diagnostic.emit({severity = "error", location = asdl.location(graph), message = message})
        return graph
    end
    local seen = {}
    for _, path in ipairs(paths) do
        if path == "" or seen[path] then
            return report("invalid physical dependency path projection")
        end
        seen[path] = true
    end
    for _, edge in ipairs(edges) do
        local importer, provider = asdl.destruct(edge)
        if not paths[importer] or not paths[provider] then
            return report("missing physical dependency endpoint")
        end
    end
    return graph
end

asdl.signlib(LIB)
return LIB
