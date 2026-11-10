-- In a shebang line: --lua=/absolute/path/to/shebang.lua
-- Programmed macros and text filters; executed before the script is read.
define("PROJECT", "MuLIB")
add_filter("trim")
function macro(name)
  if name == "YEAR" then return "2026" end
  return nil
end
function filter(line, number)
  return "[" .. tostring(number) .. "] " .. line
end
