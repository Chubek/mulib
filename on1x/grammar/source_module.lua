-- Compiler-owned transport, not a public wire format. Every location comes
-- from the host, and source strings are data, never compiled as Lua.
local function module(unit)
    local header
    if unit.header then
        header = asdl.make("module_header", nil, {name = unit.header.name}, unit.header.where)
    end

    local imports = {}
    for i, item in ipairs(unit.imports) do
        local selectors = {}
        for j, selector in ipairs(item.selectors) do
            selectors[j] = asdl.make("source_selector", nil, selector)
        end
        imports[i] = asdl.make("source_import", item.tag, {
            spelling = item.spelling, alias = item.alias, relative = item.relative,
            quoted = item.quoted, selective = item.selective, selectors = selectors,
        }, item.where)
    end

    local declarations = {}
    for i, item in ipairs(unit.declarations) do
        local visibility = asdl.make("source_visibility", item.visibility, item.fields)
        declarations[i] = asdl.make("source_decl", nil, {
            name = item.name, kind = item.kind, visibility = visibility,
        }, item.where)
    end
    return asdl.make("source_module", nil, {
        header = header, imports = imports, declarations = declarations,
    })
end

local function signature(facts)
    local members = {}
    for i, item in ipairs(facts.members) do
        members[i] = asdl.make("signature_member", item.tag, {
            name = item.name, arity = item.arity,
        }, item.where)
    end
    return asdl.make("source_signature", nil, {name = facts.name, members = members}, facts.where)
end

local function graph(facts)
    local edges = {}
    for i, edge in ipairs(facts.edges) do
        edges[i] = asdl.make("source_edge", nil, edge)
    end
    return asdl.make("source_graph", nil, {paths = facts.paths, edges = edges}, facts.where)
end

local project = ({module = module, signature = signature, graph = graph})[on1x_source_facts.kind]
local root = project(on1x_source_facts)
local checked, reason = asdl.run_logistics({transform = true, context = on1x_source_libraries}, root)
-- The native engine keeps reported diagnostics even if it rejects the root.
if not checked and asdl.diagnostic.count("error") == 0 and asdl.diagnostic.count("fatal") == 0 then
    error(reason or "source logistics did not return a checked root")
end
