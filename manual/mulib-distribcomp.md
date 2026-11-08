# mulib-distribcomp

[Manuals](README.md) · [Library index](index.md)

Distributed DAG orchestration over internal MuMQ with trusted Mune execution,
daemon control, and explicitly supplied container/OCI/VMM backends. There are no
third-party parser, MQ, crypto or Lua dependencies. A required JSON/YAML document
and a Mune task program compile to an immutable `mulib-distribtask` plan; see
that manual and `library/ldistrib` for the schema/profile helpers.

## Running

1. Create a MuMQ broker. For remote nodes use numeric IPv4 TCP listener/accept
   and connect from `mulib-socket`. Configure keys/directional nonce namespaces
   and broker topic ACLs before accepting untrusted traffic.
2. Compile the configuration + tasks, optionally loading
   `library/ldistrib/cluster.lua` first.
3. Create one coordinator with a **new, caller-generated unique run ID** and
   one worker per configured node/run. They borrow channels and queue their
   own subscriptions. Do not replace these subscriptions on the same channel.
4. Repeatedly advance broker, coordinator step(now_ms), and worker steps,
   or use an external event loop and the split tick/handle APIs. Coordinator
   tick only flushes/dispatches; it never reads. Handle accepts borrowed messages.
5. Inspect task status/finished. Destroy coordinator/workers before their plan,
   channels, callbacks and backend resources. Broker owns its accepted channels;
   coordinator/workers borrow their client channels.

`examples/modules/distribcomp.c` runs an actual end-to-end stream-socket
orchestration and prints `42`. `tests/test_distribcomp.c` additionally exercises
encrypted links, multi-node dependencies, retries, stale replies and failures.
No hidden networking, scheduling threads or persistent state are created.

## Scheduling and delivery

States are PENDING, RUNNING, SUCCEEDED, FAILED, BLOCKED and CANCELLED. At most
one task per node is RUNNING locally. All dependencies must succeed. Terminal
dependency failures propagate BLOCKED. An accepted dispatch increments attempt
and starts its lease; failed results or expired leases retry until the configured
attempt limit, then fail. Caller monotonic milliseconds must not decrease or
overflow one future lease. Timeout code is -254; unimplemented action is -253.
Worker/backend result codes are -255..255, with zero success. Status output is
borrowed until another result/destruction; copy it to retain it.

**At-least-once actions, not exactly once**: a lost request/result, a lease expiring
while work is still running, or process restart can execute an action again.
The worker caches the latest attempt/result for up to 256 IDs during its lifetime;
duplicate identical attempts resend without execution, older attempts are ignored,
and conflicting bytes for the same attempt are rejected. A fresh attempt may
execute again. Use idempotent actions or durable application deduplication.
Per-node serialization at the coordinator does not fence an already-running
timed-out worker. A run ID/attempt is correlation, not a distributed lock.

There is no durable log, automatic failover/node discovery, worker migration,
consensus, heartbeat-based membership or checkpoint recovery. Explicit node
placement is deterministic. `cancel` prevents future local dispatch and ignores
late results; it does not remotely interrupt in-flight actions. `finished` is
0 while unfinished, 1 when all succeed (also for an empty plan), -1 on terminal
failure/cancellation. Transport errors are separate from task failures and must
be handled by the caller. Reconnection requires a new coordinator/run, fresh
crypto session namespaces and a deliberate application recovery policy.

## Application wire protocol

PUBLISH topics are `cluster/<cluster>/node/<node>` for requests and
`cluster/<cluster>/result/<run>` for results. Node workers are pinned to a run;
run/node mismatches are ignored. UTF-8 JSON bodies, version 1:

```json
{"v":1,"run":"run-1","task":"start","node":"worker-a","attempt":1,
 "kind":"container","action":"start","payload":"web"}
```

Results carry v/run/task/node/attempt plus integer `code` and text `output`
(<=64 KiB). Only a matching current RUNNING attempt, before its locally observed
deadline, changes coordinator state. Malformed/wrong-type bodies fail without
coordinator mutation; stale/duplicate/unrelated results return AGAIN. Version
and attempt integers are validated. MuMQ supplies independent per-connection
framing/replay checks and authenticated encryption; its broker sees plaintext.

Broker ACLs must restrict request publications to coordinators, node-topic
subscriptions to their worker, result publications to approved workers and
result subscriptions to coordinators. A permitted malicious worker can still
lie about its result. This is a trusted computing mesh, not remote attestation.

## Worker capabilities and daemons

Default workers execute **nothing**. Set an allowlisted `execute` callback, or
explicitly enable `allow_mune` and/or `allow_daemon`. Callback arguments are
borrowed and output is an initialized `mulib_strbuf_t`; append <=64 KiB NUL-free
UTF-8, return zero or -255..255. No action is passed to a shell by distribcomp.
Oversized/NUL/invalid-UTF-8 output becomes error -255. OOM after a side effect is reported,
not silently reexecuted at the same attempt; a new attempt can still repeat it.

`mune/run` creates an independent bounded Mune state and executes payload as
trusted source. The first return value can be a string, number, boolean or nil;
strings must be NUL-free. Host library access is **not sandboxed**. Execution
is synchronous; callbacks must supply their own workload/resource/time limits.

`daemon/start` interprets payload as a `mulib-daemon` Mune specification and uses
`mulib_daemon_launch`. Up to 32 child processes are tracked by service name;
starting an already-live name succeeds without relaunch. `daemon/stop` takes
the service name as payload, sends SIGTERM via `mulib_daemon_stop` and does not
wait for shutdown. Worker steps reap exited children. Worker destruction forcibly
terminates/reaps remaining owned children; do not destroy a worker expecting its
services to stay alive. No service-manager installation or automatic restart
policy is performed. POSIX launch must be used before starting threads as
documented by daemon; do not use it from a multi-threaded application. A successful
start/stop acknowledges that operation, not readiness or process exit.

`mulib-distribcluster.h` supplies an optional allowlisted executor for prepared
container runtimes, OCI lifecycle records and VMMs. It is not pulled in by the
coordinator, keeping those large backends opt-in. See its manual for limits.

## Build

Define `MULIB_IMPL_DISTRIBCOMP`, `MULIB_IMPL_DISTRIBTASK`,
`MULIB_IMPL_MUMQ_PUBLISHER`, `MULIB_IMPL_MUMQ_SUBSCRIBER`, `MULIB_IMPL_MUMQ`,
`MULIB_IMPL_SOCKET`, `MULIB_IMPL_AES`, `MULIB_IMPL_AUTHCRYPTO`,
`MULIB_IMPL_DAEMON`, `MULIB_IMPL_PROCESS`, `MULIB_IMPL_JSON`, `MULIB_IMPL_YAML`,
`MULIB_IMPL_UTF8`, and Mune's complete switch closure listed in
`mulib-distribtask.md`, in exactly one TU. A local broker additionally needs
`MULIB_IMPL_MUMQ_SERVER`. `examples/distrib-common.h` shows explicit placement.
Linux: `cc -std=c11 -Iinclude examples/modules/distribcomp.c -lm -pthread -ldl`.
All public objects require external synchronization; callbacks must not reenter.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-distribcomp.h`](../include/mulib-distribcomp.h) |
| Implementation switch | `MULIB_IMPL_DISTRIBCOMP` |
| Repository-local includes | [mulib-daemon](mulib-daemon.md), [mulib-distribtask](mulib-distribtask.md), [mulib-mumq-publisher](mulib-mumq-publisher.md), [mulib-mumq-subscriber](mulib-mumq-subscriber.md) |
| Complete example | [`examples/modules/distribcomp.c`](../examples/modules/distribcomp.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/distribcomp.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
