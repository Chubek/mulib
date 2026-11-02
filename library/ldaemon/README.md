# LDaemon profiles and helpers

Mune sources for [`mulib-daemon.h`](../../manual/mulib-daemon.md):

* `service.lua`: returns a function configuring a daemon from a table; unknown
  fields are rejected and environment keys are sorted.
* `fswatch.lua`: returns a helper configuring an application's foreground watch
  runner with `--watch-program FILE`.
* `systemd.lua`: compilation profile returning a systemd `.service` unit from the
  host-provided `daemon` table.

Configuration helpers run inside `mulib_daemon_load`/`load_file`. The systemd
profile runs inside `mulib_daemon_compile`/`compile_file`. Neither loading a helper
nor compiling a unit implicitly installs or starts a system service.

```sh
cc -std=c11 -Iinclude examples/modules/daemon.c -pthread -lm -ldl -o /tmp/opencode/daemon
/tmp/opencode/daemon library/ldaemon/systemd.lua > /tmp/opencode/example.service
```
