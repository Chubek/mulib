-- texlua entry point for the Makefile workflow: [clean|check|export|generate]
local action = arg[1] or "build"
local commands = {
  build = "make all",
  clean = "latexmk -C -outdir=build the-book-of-mudsl.ltx",
  check = "make check",
  generate = "make generate",
  export = "python3 export_manual.py"
}
if not commands[action] then
  io.stderr:write("usage: texlua build.lua [build|clean|check|export|generate]\n")
  os.exit(2)
end
local ok, _, status = os.execute(commands[action])
if type(ok) == "number" then os.exit(ok) end
os.exit(ok and 0 or (status or 1))
