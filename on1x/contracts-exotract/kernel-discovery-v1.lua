-- Bundled, versioned ExoLib discovery profile. The build driver supplies
-- canonical, pinned inputs resolved by the shared MuLIB MANIFEST planner.
-- Never select a Lua script or a sidecar by a source-file path.
local exotract = require("exotract")
local get = exotract.global.get
local function list(key)
  local result = {}
  for i = 1, tonumber(get(key .. "_COUNT")) do
    result[i] = assert(get(key .. "_" .. i))
  end
  return result
end

local lib = exotract.exolib.new(assert(get("KERNEL")), {
  path = assert(get("HEADER")),
  documentation = assert(get("MANUAL")),
  revision = assert(get("REVISION")),
  compiler = assert(get("COMPILER")),
  clang_resource_dir = assert(get("RESOURCE")),
  cstd = "C11",
  includes = list("INCLUDES"),
  provenance_files = list("PINS"),
  required_exports = list("REQUIRED"),
  compiler_arguments = list("FLAGS"),
  defines = list("DEFINES"),
  undefines = list("UNDEFINES"),
  preprocessor = { main = exotract.preprocessor.COMPILER },
  header_only = {
    dependencies = list("DEPENDENCIES"),
    implementation = { defines = list("IMPL"), translation_units = 1 }
  }
})
local options = { engine = exotract.exobind.ON1X, safe_exports = "unsafe_explicit" }
options.contract = get("CONTRACT")
local result = lib:Preprocess():Parse():Bind(options).Internals
assert(result.unsafe == true)
-- Exotract's Lua CLI prints one returned string. All three payloads here
-- remain upstream JSON, including the unmodified embedded CDecl v2 graph.
return '{"profile":' .. result.manifest .. ',"on1x":' .. result.on1x
    .. ',"contract":' .. (result.contract or 'null') .. '}'
