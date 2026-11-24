# Native graphics wire helpers

## Status and scope

The Wayland and X11 headers provide **bounded transport, selected request
serializers, and software presentation adapters**. The Wayland adapter also has
an optional bounded, single-toplevel xdg-shell client bootstrap. This is not a
complete desktop/toolkit client or protocol-conformance claim. All contexts are
owned by one thread. The legacy configured adapters still borrow connections
and protocol objects. The optional client owns its connection and objects, but
its SHM allocation, mappings and actual allocation capacities are host-supplied.

Existing functions and members remain available. New owning objects must be
zero-initialized, must not be copied, and require their explicit free/cancel
functions. Existing synchronous send/read functions now wait through `EAGAIN`
instead of losing a partially transmitted request. This applies even when their
socket has `O_NONBLOCK`: use the new step APIs in event loops.

## Wayland transport and descriptors

`mway_message()` appends a complete native-endian request. Requests have a
nonzero object ID, an eight-byte header and a four-byte-aligned total length no
larger than 65,532 bytes. `mulib_wayland_wire_parse()` returns `1` for one complete
frame, `0` for incomplete input, and `-1` for invalid input. Output changes only
on success; `args` borrows the input.

For nonblocking transmission:

1. Zero-initialize `mway_tx`.
2. Call `mulib_wayland_wire_tx_begin()`. It copies the request and, if present,
   duplicates the source FD with close-on-exec. The caller still owns its source
   FD and may close it after a successful begin.
3. Call `mulib_wayland_wire_tx_step(tx, socket)`. Return values are `1` complete,
   `0` would block, and `-1` fatal. Retry on writable readiness, using the same
   object. The owned FD is sent only with the first successful write, never
   retransmitted with the remainder.
4. Call `mulib_wayland_wire_tx_free()`, including after failure.

Do not interleave requests on the same socket. A fatal error after a partial
write, or abandoning a partially sent request, means **discard the connection**,
not retry the request from byte zero. The helper never closes the caller's
connection. `mulib_wayland_wire_send()` is the synchronous compatibility wrapper
and returns `0/-1`, as before.

`mulib_wayland_wire_receive()` retains the `recvmsg`-style byte count.
`mulib_wayland_wire_receive_flags()` adds receive flags, including
`MSG_DONTWAIT`. A successful receive transfers every returned FD to the caller.
Received FDs are close-on-exec; Linux uses `MSG_CMSG_CLOEXEC`, with an `fcntl`
check as well. Socket acquisition and duplication use atomic close-on-exec
facilities where exposed by the target headers; portable fallbacks have the
usual fork/exec race and require application-level synchronization.

Both ancillary-buffer truncation and insufficient output FD capacity close
**all** FDs delivered by that receive, reset `fd_n` to zero, and return failure.
The bytes consumed by such a receive cannot safely be used: discard the
connection. The receive byte capacity must be nonzero, and FD/output pointers
must match their advertised capacities. EINTR is retried; EAGAIN is preserved.

### Incremental receive

`mway_stream` accumulates nonblocking input with a default 256 KiB byte limit
(`max_bytes` may be configured before receiving) and a hard limit of 32 pending
FDs. The transport byte queue and descriptor queue are deliberately separate:
Wayland FDs have **no in-band placeholder**, and recvmsg boundaries do not
identify their associated frame.

- `mulib_wayland_wire_stream_receive()` returns a positive byte count,
  `-1/EAGAIN` when no input is available, or zero on EOF. Other failures poison
  the stream and close all queued descriptors.
- `mulib_wayland_wire_stream_peek()` returns a borrowed frame. It detects
  malformed headers, declared frames exceeding the configured byte limit,
  and incomplete bytes at EOF. Receive/consume/free invalidates borrowed views.
- Decode/copy the borrowed arguments, then call
  `mulib_wayland_wire_stream_consume()` with the **FD count from that object's
  negotiated event signature**. It transfers that many FDs in order and consumes
  one frame. Return zero means more bytes/FDs are needed; do not dispatch the
  event yet or close/use untransferred FDs. Dispatch callbacks only after a
  successful consume, using copied arguments. EOF with missing FDs is fatal.
- `mulib_wayland_wire_stream_free()` closes all unconsumed descriptors.

The signature table and full object/version dispatch are still application
responsibilities. Do not guess FD counts from byte boundaries, and do not pass
buffer pointers to callbacks that outlive a peek.

## Configured Wayland SHM backend

`mulib_wayland_shm_buffer_extent()` validates four-byte pixel geometry and the
pool extent `offset + height * stride`, including final-row padding. Only standard
ARGB8888 (`0`) and XRGB8888 (`1`) are accepted by these four-byte helpers.
`mulib_wayland_shm_buffer_bounded()` additionally checks actual pool capacity.
The original buffer constructor has no pool size argument and therefore cannot
validate that capacity. No helper creates or maps a SHM allocation.

`mulib_glx_backend_wayland_t` borrows a configured surface, one or two distinct
ARGB8888 buffers, and their mappings. Source and destination strides are in
`uint32_t` words. Geometry and address-span overflow are validated, but actual
allocation capacity remains a caller contract: `mgx_surface` does not supply an
independent capacity here. Source/destination mappings and the two destination
maps may not overlap.

`mulib_glx_backend_wayland_begin()` validates every damage rectangle, builds the
complete attach/damage/commit batch, copies/premultiplies source pixels, and
marks a free buffer busy. It performs no socket I/O. Damage is clipped to the
source bounds; at most 4,096 rectangles are accepted. Failure before queueing
does not partially overwrite pixels or send requests. All-busy returns
`0/EAGAIN`; an already queued submission must be flushed before another begin.

`mulib_glx_backend_wayland_flush()` uses nonblocking short-write-safe transport:
`1` means requests sent, `0` means retry on writable readiness, `-1` disables the
backend. The source may change after successful begin because its pixels have
already been copied. `mulib_glx_backend_wayland_present()` is the synchronous
begin/flush compatibility wrapper.

**A queued or committed buffer stays busy until a real `wl_buffer.release`
event**, dispatched by the caller to `mulib_glx_backend_wayland_release()`.
Unknown/zero IDs are ignored. A `wl_surface.frame` callback is not buffer release,
and neither event guarantees presentation-time feedback. `flush()` success is
not display completion. Configure/ack and frame/release event dispatch are not
automatic.

On send failure or `mulib_glx_backend_wayland_cancel()`, busy state is retained
conservatively. Disconnect the borrowed connection before destroying/reusing
compositor-owned mappings. Cancel frees pending request storage but never
closes the caller's socket or frees mappings.

Other serializers validate mandatory object/FD arguments. DMA-BUF helpers only
emit selected requests: plane formats/modifiers, capabilities, version support
(including create-immed's required version), allocation sizes and synchronization
must still be negotiated. Text-input-v3 surrounding text is limited to 4,000
bytes, scalar-valid UTF-8, with cursor/anchor at byte boundaries between
codepoints; it is not an IME implementation.

## Optional Wayland toplevel client

`mulib_glx_backend_wayland_client_t`, in `mulib-glx-backend-wayland.h`, adds a
single-owner, callback-free state machine above the existing `mway_tx` and
`mway_stream` transports. Existing backend members/functions are unchanged.
Zero-initialize the client and never copy a live context. Do not mix its I/O
with raw requests or a libwayland event dispatcher.

### Bootstrap and pumping

- `..._client_init(c, connected_fd, title, width, height, max_pool_bytes)` queues
  `get_registry` and `sync` without I/O. **Success transfers socket ownership**;
  failure leaves it caller-owned. The socket is marked close-on-exec.
- `..._client_connect(c, explicit_socket_path, ...)` obtains its own Unix socket.
  It performs a synchronous `connect`; subsequent I/O is nonblocking. It does
  not select the active display from environment variables or launch a server.
- On readable readiness call `..._client_receive`: `1` means receive progress
  (including queued EOF), `0` means EAGAIN, and `-1` is fatal.
- Call `..._client_dispatch(c, budget)` to process buffered events. It returns
  their count or `-1` on failure. Budget is `1..1024`. When the budget is
  exhausted, reschedule dispatch **without waiting for another readable edge**.
  EOF is reported after already-buffered complete events; truncated EOF fails.
- Call `..._client_flush`: `1` drained, `0` EAGAIN, `-1` fatal. Watch writability
  only while `control.bytes.n` or `backend.tx.bytes.n` is nonzero. Partial control
  and frame batches are never interleaved, including when ping/configure events
  arrive during a partial frame transmission.

The registry roundtrip selects `wl_compositor`, `wl_shm`, and `xdg_wm_base`,
binding **version 1**, even when higher versions are advertised. It creates a
surface, xdg_surface and toplevel, sets the caller's UTF-8 title (at most 255
bytes), and performs the initial **bufferless** commit. Ping produces pong.
Toplevel configure and xdg_surface configure update `width`/`height` and queue
ack_configure. Zero dimensions retain the host-selected size. Configure may
arrive again while a previous frame/buffer is pending; the newest accepted
geometry wins. `resize_pending` signals that new matching buffers are needed.
`close_requested` prevents further frame submissions.

### Host SHM storage and release-safe resize

After `state == MWAY_CLIENT_READY` and receipt of the ARGB8888 format, supply:

```c
mulib_glx_backend_wayland_client_buffers(
    &client, pool_fd, pool_bytes, offsets, maps, capacities_in_words);
```

There must be exactly two disjoint writable `mgx_surface` mappings of this FD,
at the supplied byte offsets, matching the accepted dimensions at scale one.
Word strides, full padded-row capacities, pool extent, address overflow, and
the FD's current file size are checked. The host guarantees the mapping/FD
relationship, which cannot be proved from pointers. The host also guarantees
that the file is not truncated while in use.

This queues old buffer destruction (on replacement), a new SHM pool and two
ARGB8888 buffers, then pool destruction. The original FD remains caller-owned;
a close-on-exec duplicate is held until SCM_RIGHTS is sent or teardown occurs.
The caller may close the original FD immediately after success. The client
never unmaps or frees supplied storage.

Replacement returns `0/EBUSY` while **either old buffer is busy**, a frame
callback remains outstanding/unconsumed, or transmission is pending. A
configure acknowledgement does not release storage. Retry after processing
release/frame events and draining transmission. Only after successful
replacement may the old maps be unmapped. Socket disconnection is the alternate
quiescence path.

### Submission and frame pacing

`..._client_begin(c, source, damage, count, token)` copies/premultiplies into a
free SHM buffer and queues attach/damage/**frame**/commit. It requires a nonzero
host token, drained control traffic, and accepted matching geometry. At most
one frame callback is outstanding. Buffers remain independently busy until their
actual `wl_buffer.release`. Allocation failure never accepts the token or leaks
a busy slot; a free map may already have been overwritten before rollback.

`..._client_take_frame(c, &token, &milliseconds)` returns each frame callback
once. The token can identify the pending `mulib_glx_present_t` serial; the host
may then call `mulib_glx_present_complete` or `mulib_glx_window_complete`.
This is **frame pacing/CPU-source completion**, not physical presentation
feedback. It must not release a SHM mapping. The source pixels themselves were
copied before successful begin returned.

Malformed events, unexpected FDs (all bound v1 event signatures are FD-free),
unknown object IDs, duplicate releases/deletion acknowledgements, required-global
removal and transport errors close the owned socket and free transport queues.
`..._client_free` also disconnects **before** releasing queues; it never frees
borrowed maps. After backend quiescence, `mulib_glx_present_abort` or
`mulib_glx_window_abort` can abandon a pinned source without recording a
successful completion. Never use abort on a still-live reader.

Limits: 8192 per dimension; caller-supplied nonzero pool-byte ceiling no greater
than `INT32_MAX`; 256 registry announcements; 64 outstanding `delete_id`
acknowledgements; 64 KiB control TX; default 256 KiB RX; and 4096 damage
rectangles per frame. IDs increase monotonically and are never reused.
There is no seat, keyboard/XKB, output/DPI, clipboard, popup, subsurface,
presentation-time, DMA-BUF, reconnect, multiwindow or compositor-server support
in this client. Required-global removal fails closed rather than recovering.

## X11 transport, framing and images

All X11 serialization here uses the little-endian (`'l'`) connection order.
`mx11_read_step()` and `mx11_write_step()` use a caller-owned offset and return
`1` complete, `0` would block, `-1` fatal. Preserve the offset and buffer until
completion; EOF before completion is an error. Their old `mx11_read()` and
`mx11_write()` wrappers wait synchronously.

`mx11_request_tx` owns a copied header and payload:
`mulib_x11_wire_request_begin()`, `..._step()`, `..._free()` provide resumable
requests. Sequence increments exactly once, only after full transmission.
Request lengths must fit both the server-advertised core request limit and the
16-bit core length field; BIG-REQUESTS is not implemented. A failed send, or
freeing an incomplete request after bytes were sent, marks `connection.failed`.
Do not interleave requests. The original `mulib_x11_wire_request()` is the
synchronous wrapper.

Setup validates the complete bounded vendor/pixmap-format/screen/depth/visual
tables before exposing the reply allocation. Authentication or malformed setup
is reported as failure, not a usable connection. The resource-ID allocator
handles sparse masks and saturates on exhaustion without wrapping/reusing IDs.
Socket discovery and Xauthority selection are not implemented.

`mulib_x11_wire_parse()` frames 32-byte errors/core events and variable-length
replies/XGE events. It returns `1/0/-1`, with a configurable per-frame size limit
(zero selects 16 MiB). The caller retains partial bytes and performs
reply/error/extension demultiplexing; this parser does not read a socket.

`mulib_x11_events_decode_bounded()` requires at least 32 bytes and leaves output
unchanged on unsupported/malformed events. Keycodes are **native X11 keycodes**,
not evdev codes or keysyms; actual XKB translation is external. The legacy
32-byte decoder retains its historical keycode-minus-eight behavior, which
must not be treated as universal keyboard mapping.

`mulib_x11_image_setup_format()` checks an advertised depth-24/32, 32bpp,
32-bit-scanline-pad, LSBFirst image format. It does not select or validate the
drawable's visual/channel masks. `mulib_x11_image_put()` and the configured X11
backend require that format and caller-verified compatible RGB masks. They
validate stride arithmetic, dimensions and request limits, and split uploads
into whole-row requests without copying stride padding. Other depths, layouts,
and visuals are unsupported; alpha is not composited for the caller.
The backend copies/sends the source before returning. Successful PutImage
transmission is not server processing or presentation completion.

Property helpers validate element size and the server request limit before
allocating/sending. Property elements of format 16/32 must already be in wire
byte order. Selection requests are not clipboard INCR or ICCCM/EWMH support.

## Reproducible tests

Run from the repository root:

```sh
cc -std=c11 -pedantic -Wall -Wextra -Werror -Ikernels \
  tests/test-gfx-wire.c -lm -o /tmp/opencode/test-gfx-wire
/tmp/opencode/test-gfx-wire

cc -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Wpedantic -Werror -Ikernels \
  tests/test-gfx-backend-followup.c -lm -ldl -o /tmp/opencode/gfx-backend-followup
/tmp/opencode/gfx-backend-followup

cc -std=c11 -pedantic -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g -Ikernels \
  tests/test-gfx-wire.c -lm -o /tmp/opencode/test-gfx-wire-sanitize
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /tmp/opencode/test-gfx-wire-sanitize
```

The tests use nonblocking Unix socketpairs, reduced send buffers, pipes and
`/proc/self/fd` accounting. They exercise short writes/resumption, exactly-once
SCM_RIGHTS delivery, borrowed/owned FD lifetimes, CLOEXEC, output-capacity and
ancillary truncation cleanup, malformed/truncated frames, queued-FD cleanup on
EOF/error, request/ID exhaustion, setup bounds, XGE bounds, padded strides,
premultiplication, clipped damage and busy/release/failure states. They never
open display servers, `/dev/dri` or `/dev/input`.

`test-gfx-backend-followup.c` additionally uses a private Unix listener and
socketpair compositor fixtures to check the actual registry/bind/initial
configure exchange, partial headers, bounded dispatch, SHM FD transfer, real
file mappings, frame-versus-release ordering, release-blocked resize, request
ordering under backpressure, allocation-failure rollback, malformed events,
unexpected FD cleanup, backend-loss abort, and disconnect-before-unmap teardown.
Run the same command with Clang and with
`-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`.
No live compositor or external/public display connection is used by these tests.

## Remaining integration gates

- Generate/validate protocol signatures from upstream XML; extend the bounded
  v1 toplevel client with seat/XKB/IME, outputs/DPI, host SHM allocation policy,
  popups/subsurfaces, multiwindow handling and recovery.
- Test resize, release ordering, disconnection and multiwindow behavior under a
  nested compositor and a real Wayland session before calling this a desktop
  backend. DMA-BUF, clipboard and presentation feedback need separate live tests.
- Test setup/root/visual selection, server errors and replies, WM_DELETE_WINDOW,
  XKB, clipboard INCR, resize and disconnect under Xvfb/Xephyr. Real big-endian
  servers/visuals, other pixel layouts, MIT-SHM and BIG-REQUESTS are not covered.
- No live compositor/X server, 32-bit runtime, service/session integration or
  GPU validation is claimed by the socketpair suite.
