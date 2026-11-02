-- Configuration helper, loaded with dofile or bundled as ldaemon.service.
return function(spec)
  assert(type(spec) == "table", "expected service table")
  for key in pairs(spec) do
    assert(key == "name" or key == "description" or key == "argv"
        or key == "directory" or key == "restart" or key == "environment",
        "unknown service field: " .. key)
  end
  assert(spec.name, "name required")
  daemon_name(spec.name)
  if spec.description then daemon_description(spec.description) end
  if spec.directory then daemon_directory(spec.directory) end
  if spec.restart then daemon_restart(spec.restart) end
  assert(spec.argv, "argv required")
  local count = 0
  for key in pairs(spec.argv) do
    assert(type(key) == "number" and key % 1 == 0 and key >= 1 and key <= #spec.argv,
        "argv must be a dense array")
    count = count + 1
  end
  assert(count == #spec.argv, "argv must be a dense array")
  daemon_command(table.unpack(spec.argv))
  -- Sort keys so generated units are reproducible across table iteration orders.
  local keys = {}
  for key in pairs(spec.environment or {}) do keys[#keys + 1] = key end
  table.sort(keys)
  for _, key in ipairs(keys) do daemon_environment(key, spec.environment[key]) end
end
