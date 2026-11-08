# mulib-channel

[Manuals](README.md) · [Library index](index.md)

Thread-safe CSP channels that also suspend MuLIB fibers cooperatively. Define
`MULIB_IMPL_CHANNEL`, `MULIB_IMPL_FIBER` and `MULIB_IMPL_XTHREADS` in exactly one
translation unit each, before including headers. Link POSIX builds with
`-pthread`. See [fiber platform requirements](mulib-fiber.md).

## Channels and directional views

`mulib_channel_create(&channel, sizeof(T), capacity)` creates a bidirectional
channel copying fixed-size values. Capacity 0 is an **unbuffered rendezvous**:
send completes only when a receiver takes the value. Positive capacity gives a
bounded FIFO. Once full, senders wait; once empty, receivers wait.

Use `mulib_channel_sender(channel)` and `mulib_channel_receiver(channel)` to
obtain distinct send-only and receive-only view types:

```c
int value = 42;
mulib_channel_send(mulib_channel_sender(channel), &value, MULIB_XWAIT_FOREVER);
mulib_channel_receive(mulib_channel_receiver(channel), &value, MULIB_XWAIT_FOREVER);
```

The owner retains the bidirectional handle; functions accepting one view type
cannot accidentally receive the other type. C's publicly visible struct fields
are not a security boundary. Views are borrowed and do not keep the channel alive.

Send/receive timeouts are **relative milliseconds**, with 0 for a single try and
`MULIB_XWAIT_FOREVER` for indefinite wait. Blocked fiber calls park their fiber;
ordinary native-thread calls wait on conditions. Sources/destinations must remain
valid and exclusively available for the duration of the operation. A successful
send copies bytes, including raw pointers, without managing nested ownership.

## Go-like select

Construct an array with `mulib_channel_send_case(sender, &input)` and
`mulib_channel_receive_case(receiver, &output)`. Then call:

```c
size_t selected;
int status = mulib_channel_select(cases, count, has_default, timeout_ms, &selected);
```

Select commits **exactly one** ready operation and returns that operation's
status and array index. Closed channels are ready: closed sends return CLOSED;
drained closed receives return CLOSED and zero the output. Buffered closed
channels first return queued values with OK. The remaining cases have no effect.
Case arrays and every value buffer must remain alive and unchanged until return.
Do not overlap buffers belonging to concurrently executing operations.

Ready cases receive rotating priority rather than Go's pseudo-random choice.
There is no strict fairness guarantee across competing callers. Waiting cases
are queued FIFO per channel. Two blocked selectors can rendezvous directly;
duplicate channel cases are supported, and a select never matches its own send
and receive. All losing registrations are removed before the call returns.

If no operation is ready and `has_default` is nonzero, return DEFAULT immediately
regardless of timeout. Otherwise wait until one commits or return TIMEOUT.
Default/timeout/error without a committed case sets `selected` to `SIZE_MAX`.
The index output is required. NULL channels disable their cases, like Go nil
channels; their data pointers may be NULL. Zero cases/all-nil cases can default,
time out, or wait forever. Invalid operation tags and NULL active data pointers
are rejected before anything is committed. Timeout leaves outputs unchanged.

## Close, lifetime and failures

`mulib_channel_close(channel)` wakes waiters and rejects future sends. Buffered
values remain available in order. After draining, receives return CLOSED and
zero exactly one element's bytes (this is byte zeroing, not a portable semantic
constructor for every C object type). Unlike Go's panic behavior, duplicate
close and send-after-close return CLOSED. NULL close is INVALID.

There is no automatic reference counting. Close, then join/wait for all users,
then destroy. `mulib_channel_destroy(NULL)` is harmless; destroy returns BUSY
while registrations remain, including notified waiters that have not returned.
The owner must exclude concurrent new operations and discard all borrowed views
before freeing the channel. Destruction does not run element destructors.

Results are `MULIB_CHANNEL_OK/INVALID/NOMEM/SYSTEM/CLOSED/TIMEOUT/DEFAULT/BUSY`.
Create rejects zero element size and multiplication overflow, clearing `*out`.
Allocation failure leaves no channel/registration behind. Blocking select
allocates O(case count) wait records, while immediately ready operations do not.
Payload storage is O(element size × capacity). One process-local implementation
mutex linearizes operations across channels, trading throughput scalability for
atomic multi-channel selection. No callbacks execute under it. Do not use native
thread cancellation to interrupt these APIs.

See [`examples/modules/channel.c`](../examples/modules/channel.c).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-channel.h`](../include/mulib-channel.h) |
| Implementation switch | `MULIB_IMPL_CHANNEL` |
| Repository-local includes | [mulib-fiber](mulib-fiber.md) |
| Complete example | [`examples/modules/channel.c`](../examples/modules/channel.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/channel.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
