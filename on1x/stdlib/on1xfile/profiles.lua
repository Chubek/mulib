-- Public Mune API for root-owned On1xfile build policy.
local fields = __on1x_context or {}
local target_sources = __on1x_targets or {}
local arguments = __on1x_arguments or {}
__on1x_context, __on1x_targets, __on1x_arguments = nil, nil, nil
local project, kind, selected_profile = data.project, data.kind, data.profile
local pairs_ = pairs

local function context()
    local targets = {}
    for key, source in pairs_(target_sources) do
        targets[key] = source
    end
    return {
        project = project, kind = kind, profile = selected_profile,
        backend = fields.backend, directory = fields.directory,
        target = fields.target, targets = targets,
        compiler = fields.compiler, assembler = fields.assembler, linker = fields.linker
    }
end

local function argument(key)
    assert(type(key) == "string", "profile argument name must be a string")
    return arguments[key]
end

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

on1xfile = { profile = profile, argument = argument, context = context }
return {
    dev = profile { optimize = 0, debug = true },
    release = profile { optimize = 3, debug = false }
}
