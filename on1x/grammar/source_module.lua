-- Embedded host adapter for the parser's source facts. Every location comes
-- from the host, and source strings are data, never compiled as Lua.
local unit = on1x_source_facts
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

local root = asdl.make("source_module", nil, {
    header = header, imports = imports, declarations = declarations,
})
local checked, reason = asdl.run_logistics({transform = true}, root)
-- The native engine keeps reported diagnostics even if it rejects the root.
if not checked and asdl.diagnostic.count("error") == 0 then
    error(reason or "source logistics did not return a checked root")
end
