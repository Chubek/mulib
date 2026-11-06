-- Portable alternative to make: texlua build.lua [clean|check|export]
local action = arg[1] or "build"
local commands = {
  build = "latexmk -lualatex -interaction=nonstopmode -halt-on-error -outdir=build the-book-of-mudsl.ltx",
  clean = "latexmk -C -outdir=build the-book-of-mudsl.ltx",
  check = "python3 export_manual.py --check",
  export = "python3 export_manual.py"
}
if not commands[action] then
  io.stderr:write("usage: texlua build.lua [build|clean|check|export]\n")
  os.exit(2)
end
local ok, _, status = os.execute(commands[action])
if type(ok) == "number" then os.exit(ok) end
os.exit(ok and 0 or (status or 1))
