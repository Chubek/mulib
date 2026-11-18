-- Public Mune API for root-owned On1xfile build policy.
local function profile(options)
    assert(type(options) == "table", "profile options must be a table")
    for key in pairs(options) do
        assert(key == "optimize" or key == "debug", "unknown profile option")
    end
    local optimize, debug = options.optimize, options.debug
    assert(math.type(optimize) == "integer" and optimize >= 0 and optimize <= 3,
           "profile optimize must be an integer from 0 through 3")
    assert(type(debug) == "boolean", "profile debug must be boolean")
    return { optimize = optimize, debug = debug }
end

on1xfile = { profile = profile }
return {
    dev = profile { optimize = 0, debug = true },
    release = profile { optimize = 3, debug = false }
}
