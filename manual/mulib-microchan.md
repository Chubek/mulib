# MicroChan

[Manuals](README.md) · [Weblets](mulib-muw3-mune.md)

**Header:** `include/mulib-microchan.h` · **Switch:** `MULIB_IMPL_MICROCHAN`

MicroChannel is a local, binary-safe publish/subscribe protocol with correlated
acknowledgements. The header supplies a portable codec, a Unix-stream client and
an embeddable nonblocking broker. `library/microchan` supplies the Mune-configured
daemon, using MuLIB's daemon/process tools. Exact topics have no wildcard matching.

## Integration and errors

Define `MULIB_IMPL_MICROCHAN`, `MULIB_IMPL_CONTAINER_IPC`, `MULIB_IMPL_CRC32`, and
`MULIB_IMPL_SOCKET`. Container-core helpers are static inline. No third-party
dependencies. `library/microchan/runtime.c` is a ready-made implementation TU;
CMake target `mulib_microchan` compiles it separately.

All statuses use `MULIB_CT_*`: zero success; AGAIN=1 for incomplete input/timeouts;
negative errors distinguish invalid arguments, malformed frames, CRC failures,
allocation, bounds, missing platform support and I/O. `mulib_ct_strerror` explains
them. The broker communicates operation failures in `message.status` inside ACKs.

## Wire protocol version 1

MicroChan reuses the checksummed **MCIP v1** framing from `mulib-container-ipc.h`:

| Offset | Width | Meaning |
| --- | --- | --- |
| 0 | 4 | ASCII MCIP |
| 4 | 1 | envelope version 1 |
| 5 | 1 | response flag (0/1) |
| 6 | 2 | opcode, little-endian |
| 8 | 8 | nonzero correlation ID, little-endian |
| 16 | 4 | payload bytes, little-endian |
| 20 | 4 | signed CT status encoded modulo 2^32, little-endian |
| 24 | 4 | IEEE CRC32 of payload |
| 28 | 4 | IEEE CRC32 of first 28 header bytes |
| 32 | 1 | MicroChan payload version 1 |
| 33 | 1 | topic byte count (1..127) |
| 34 | topic count | ASCII topic, no NUL |
| following | remaining | binary content |

Topics use letters, digits, `_`, `-`, `.`, `/`. Opcodes: SUBSCRIBE=0x4d01,
UNSUBSCRIBE=0x4d02, PUBLISH=0x4d03, EVENT=0x4d04. Requests have response/status=0.
Subscribe/unsubscribe carry no content. ACKs echo request opcode/topic/ID with
response=1, no content, and status=0 or a negative CT result. Events have
response=1/status=0 and preserve publisher ID/topic/content. IDs are correlation
values, not globally unique identities. An EVENT never requires an ACK.

Unsupported versions/opcodes, unknown flags, zero IDs, malformed topics, wrong
operation shapes, oversized payloads and bad CRCs fail. CRC is corruption detection,
not authentication. The protocol does not implement durable queues, replay or
exactly-once delivery across disconnects.

`encode` transactionally replaces an initialized `mulib_ct_blob`; input can alias
the old output. `decode` parses one frame, leaves output unchanged on failure and
sets consumed=0. On success, topic is copied into the message and content borrows
the input. Pipelined bytes after consumed belong to later frames. A zero payload
limit selects 1 MiB; the supported maximum is 16 MiB, excluding protocol overhead.

## Client

`mulib_microchan_open(&client,path,max_payload,timeout_ms)` requires `client=NULL`
and connects a close-on-exec, nonblocking Unix socket. Zero send/connect timeout
selects 30 seconds per wait. `send` writes a complete caller-supplied message;
partial-write errors close the connection, preventing accidental frame reuse.

`receive(client,timeout_ms,&message)` returns the next acknowledgement or event.
Zero timeout polls. Partial frames survive timeouts. Binary content is borrowed
until the next receive or close; copy it if retained. Clean or truncated EOF is
an I/O error; reconnect explicitly. Client calls require external serialization.
`close` releases the client and its reader; NULL is harmless, a freed pointer must
not be used again. Applications choose nonzero IDs and correlate acknowledgements;
events may arrive before an outstanding operation's ACK.

## Broker and backpressure

`broker_create(options)` copies a namespace prefix and applies zero defaults:
1 MiB content, 4 MiB queued wire bytes per client, 64 clients, 32 subscriptions per
client. Maximums are 16 MiB content, 64 MiB queue, 1024 clients and 1024 subscriptions.
Queue capacity must fit a maximum frame (content+161 bytes). Allocation remains
bounded by configured clients, readers, topic slots and queues.

`broker_listen` creates a new filesystem socket, chmods it to 0600 and never
unlinks an existing entry. Put it in a private directory (0700). Prefix matching
is literal, so `muw3/` includes descendants but not `muw3-other/`. It is a namespace
policy shared by all peers, not a peer-identity ACL.

`broker_step(ms)` polls once, accepts at most one client, and performs at most one
read/write per peer per round. Thus fragmented or stalled peers cannot monopolize
the broker. The caller owns scheduling and signal handling. Duplicate subscribe
and missing unsubscribe are idempotent. Disconnect automatically releases topics.

Publication first reserves every recipient queue and the publisher's ACK. If any
recipient lacks capacity, no event is queued anywhere and the publisher receives
ELIMIT. On success, all subscribed clients (including the sender) receive the
event; sender's ACK precedes its own event. ACK means queued in memory, not consumed
or persisted. Invalid peers are closed in isolation; a client whose own ACK cannot
fit is closed. Idle/slow peers occupy one bounded slot; applications may set process
resource limits and choose retry/drop policy on ELIMIT.

Destroy closes every peer and removes the owned path only if its device/inode
still matches. Portable codec operations work without POSIX; native broker/listen
operations return ENOTSUP where Unix sockets are unavailable.

## Daemon and MuW3 profile

```
microchand --socket /absolute/private/bus.sock
           [--profile library/microchan/profiles/muw3.prof] [--daemon]
```

Profiles are Mune programs returning one table with `topic_prefix`, `max_payload`,
`max_queue_bytes`, `max_clients`, `max_subscriptions`. Unknown keys/wrong types fail.
Profiles have Mune's ordinary host capabilities. `muw3.prof` selects `muw3/` topics
and the defaults above. The C broker itself has no Mune dependency.

Foreground mode works under service managers. `--daemon` uses
`mulib_daemon_launch` (session setup, cwd, stdio, child PID reporting); process setup
success precedes socket readiness. The service uses umask 077. SIGTERM/SIGINT causes
bounded polling shutdown and path cleanup; abrupt termination can leave a socket
path that the operator must remove after confirming the daemon is gone.

See [daemon README](../library/microchan/README.md),
[`examples/modules/microchan.c`](../examples/modules/microchan.c),
`tests/test-microchan.c`, and `tests/test_muw3_integration.py`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-microchan.h`](../include/mulib-microchan.h) |
| Implementation switch | `MULIB_IMPL_MICROCHAN` |
| Repository-local includes | [mulib-container-ipc](mulib-container-ipc.md), [mulib-socket](mulib-socket.md) |
| Complete example | [`examples/modules/microchan.c`](../examples/modules/microchan.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/microchan.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
