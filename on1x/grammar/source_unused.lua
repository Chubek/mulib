LIB = asdl.lib.new("on1x.source.unused")
asdl.lib.version(LIB, "1")

function LIB.Report(D, binding)
    local name, kind = asdl.destruct(binding)
    local tag = asdl.tag(binding)
    local function warn(code, format, diagnostic_name)
        D.Warn(D.FaultPattern(code, format, {name = diagnostic_name, group = "unused"}),
               binding, kind, name)
    end
    if tag == "UnusedImport" then
        warn("ON1X-UNUSED-IMPORT", "unused import of %s '%s'", "unused-import")
    elseif tag == "UnusedVariable" then
        warn("ON1X-UNUSED-VARIABLE", "unused %s '%s'", "unused-variable")
    elseif tag == "UnusedType" then
        warn("ON1X-UNUSED-TYPE", "unused private %s '%s'", "unused-type")
    end
    return binding
end

asdl.signlib(LIB)
return LIB
