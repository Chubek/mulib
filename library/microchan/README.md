# MicroChan daemon

```sh
make muw3
mkdir -m 700 /tmp/opencode/my-weblets
build/microchand --socket /tmp/opencode/my-weblets/bus.sock \
  --profile library/microchan/profiles/muw3.prof
```

Run foreground under a supervisor, or add `--daemon` to launch through
`mulib_daemon_launch`. The latter prints the child PID after process setup; socket
readiness is separate. SIGINT/SIGTERM closes peers and removes the owned socket.
An existing path is never overwritten. Use an absolute path in a private directory.

The profile is a Mune program returning a table of broker limits and `topic_prefix`.
The supplied profile permits `muw3/` topics and 1 MiB binary payloads. Unknown keys,
wrong types and invalid limits fail startup. It is a namespace/resource profile,
not a service-manager template or a per-client identity system. Local access uses
the socket's mode 0600 and its containing directory's permissions.

Start [worker.lua](../lmuw3/examples/worker.lua), then
[client.lua](../lmuw3/examples/client.lua) to send an HTTP job between Weblets.
The transport-independent codec and embeddable poll-driven broker are also usable
from C without Mune. See the [protocol and API manual](../../manual/mulib-microchan.md).
