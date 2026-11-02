-- Mune profile: compile the global daemon specification to a systemd unit.
-- argv is passed directly to exec; shell quoting is deliberately not involved.
local d = daemon
local function scalar(s)
  assert(utf8.len(s), "systemd fields must be valid UTF-8")
  return (s:gsub("%%", "%%%%"))
end
local function raw(s)
  -- Scalar directives do not unquote or C-unescape their values. A final space
  -- prevents a trailing backslash from joining the following physical line;
  -- systemd trims that space when parsing the scalar value.
  assert(s:sub(1, 1) ~= " " and s:sub(-1) ~= " ",
      "systemd scalar fields cannot preserve boundary spaces")
  return scalar(s) .. " "
end
local function quote(s, command)
  s = scalar(s):gsub("\\", "\\\\"):gsub('"', '\\"')
  if command then s = s:gsub("%$", "$$") end
  return '"' .. s .. '"'
end
local command = {}
for index, arg in ipairs(d.argv) do command[#command + 1] = quote(arg, index ~= 1) end
local lines = {
  "[Unit]",
  "Description=" .. raw(d.description),
  "",
  "[Service]",
  "Type=simple",
  "WorkingDirectory=" .. raw(d.directory),
  "ExecStart=" .. table.concat(command, " "),
  "Restart=" .. d.restart,
  "UMask=0027",
}
for _, entry in ipairs(d.environment) do
  lines[#lines + 1] = "Environment=" .. quote(entry, false)
end
lines[#lines + 1] = ""
lines[#lines + 1] = "[Install]"
lines[#lines + 1] = "WantedBy=multi-user.target"
return table.concat(lines, "\n") .. "\n"
