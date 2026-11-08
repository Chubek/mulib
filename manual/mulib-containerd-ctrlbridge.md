# mulib-containerd-ctrlbridge

[Manuals](README.md) · [Library index](index.md)

## NAME AND INTEGRATION

Incremental authorized daemon control connection with output backpressure and
last-request replay handling. Define `MULIB_IMPL_CONTAINERD_CTRLBRIDGE` and the
`containerd`/`containerd-accessstub` closures. Example:
`examples/modules/containerd-ctrlbridge.c`.

## CONNECTION LIFETIME

`init(bridge,daemon,policy,principal)` borrows a live daemon/policy and copies the
transport-authenticated principal. One bridge represents one connection and one
request-ID sequence. Keep borrowed objects alive until bridge free. No sockets,
credentials or background worker are created.

`feed(bytes,length,&consumed,now)` accepts arbitrary fragments through one 96-byte
command payload. DONE means a reply is pending. During pending output it consumes
zero and returns AGAIN, allowing the caller to apply stream backpressure. Excess
input after one request remains caller-owned for a later call. Server now replaces
wire command time before authorization and dispatch.

`drain(output,capacity,&written)` copies a reply prefix; AGAIN means more output or
no reply yet, DONE means the current reply was fully drained. Output may be
drained in single-byte chunks. Zero capacity produces zero progress.

## REPLAY AND EXECUTION

Request IDs must be nonzero and increase per connection. The exact most recent
request bytes and response are cached. An exact retry of that ID replays its
response without invoking authorization or execution again. An older ID or reuse
with different bytes is ESTALE and poisons the connection. This is a one-entry
connection-local retry cache, not persistent exactly-once delivery across reconnects.

All response storage (80 bytes maximum) is embedded in the bridge before any
side-effecting command executes. Thus response-allocation failure cannot lose a
successfully executed command. Authorization errors and daemon errors become
correlated negative-status replies with empty payloads.

## ERRORS AND RELEASE

Framing, checksum, malformed-command, allocation and replay errors are sticky;
close/free the connection rather than attempting byte-level resynchronization.
busy rejects callback reentry. `free` releases the incremental accumulator and
resets the bridge, without touching daemon/policy. Transport authentication and
confidentiality remain explicit application responsibilities.

Tests exercise fragmented requests/replies, stale generations, cached START replay,
policy denial, mount-lease deletion rejection and complete client/server composition.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-containerd-ctrlbridge.h`](../include/mulib-containerd-ctrlbridge.h) |
| Implementation switch | `MULIB_IMPL_CONTAINERD_CTRLBRIDGE` |
| Repository-local includes | [mulib-containerd-accessstub](mulib-containerd-accessstub.md), [mulib-containerd](mulib-containerd.md) |
| Complete example | [`examples/modules/containerd-ctrlbridge.c`](../examples/modules/containerd-ctrlbridge.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/containerd-ctrlbridge.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
