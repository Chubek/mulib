-- Return a configuration helper for an application's foreground FSWatch runner.
-- The runner accepts --watch-program FILE and owns dispatching rule handlers.
return function(name, runner, program)
  daemon_name(name)
  daemon_description("Filesystem watcher: " .. name)
  daemon_command(runner, "--watch-program", program)
  daemon_restart("on-failure")
end
