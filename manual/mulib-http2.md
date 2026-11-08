# HTTP/2 frames

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_HTTP2` and `MULIB_IMPL_BUFFER`. The codec reads and writes
the nine-byte frame envelope and validates known frame payloads. It implements
neither HPACK nor a connection/stream state machine.

`mulib_http2_frame_read(data, length, max_payload, &frame, &consumed)` returns
1 OK, 0 MORE, -1 invalid, or -2 limit. `max_payload` is explicit: zero permits
only empty payloads; use 16384 for HTTP/2's initial maximum frame size or the
appropriate negotiated maximum. The advertised size is checked before waiting
for the payload. Both output arguments remain unchanged on any non-success.
On success the payload borrows input and consumed identifies exactly one frame;
trailing bytes are available for the next call. The reserved stream-id bit is
ignored on input, as required by HTTP/2.

`mulib_http2_frame_write(&buffer, &frame)` returns 1 on success, 0 on invalid
input or allocation failure. Initialize the output buffer first. Appending is
transactional, including when payload points into the existing output buffer.
The writer requires a 31-bit stream ID and a payload length of at most 0xffffff;
it clears the reserved header bit. It does not apply the peer's negotiated limit.

## Frame-local validation

* DATA, HEADERS, PRIORITY, RST_STREAM, PUSH_PROMISE and CONTINUATION require
  nonzero stream IDs. SETTINGS, PING and GOAWAY require stream zero.
* Fixed/minimum payload lengths are enforced for PRIORITY, RST_STREAM, SETTINGS,
  PING, GOAWAY and WINDOW_UPDATE. SETTINGS ACK must be empty.
* Padded DATA/HEADERS/PUSH_PROMISE require a pad-length byte and sufficient
  space for all padding and any priority/promised-stream fields. Empty data with
  padding is valid. Padding bytes themselves need not be zero.
* PRIORITY and HEADERS priority dependencies cannot refer to the frame's own
  stream; the exclusive bit is masked when comparing. PUSH_PROMISE must specify
  a nonzero promised stream, ignoring its reserved bit.
* WINDOW_UPDATE increments must be nonzero after masking the reserved bit.
* SETTINGS_ENABLE_PUSH (2), SETTINGS_ENABLE_CONNECT_PROTOCOL (8) and
  SETTINGS_NO_RFC7540_PRIORITIES (9) must be 0 or 1. INITIAL_WINDOW_SIZE (4)
  is at most 2^31-1; MAX_FRAME_SIZE (5) is in 16384..16777215.
* Repeated SETTINGS identifiers are **legal in HTTP/2**; consumers apply them
  in wire order. Unknown setting identifiers, frame types and flag bits are
  preserved rather than rejected.

Endpoint-role restrictions (such as server-sent ENABLE_PUSH), stream parity,
promised-stream availability, continuation sequencing, header compression,
flow-control accounting and settings changes over time need a higher-level
connection implementation. Frame-local validation is not a complete HTTP/2 peer.

[Example](../examples/modules/http2.c). Tests:
`tests/test_http_hardening.c`, `tests/test_http_alloc.c`, and HTTP/2 mutation/
roundtrip coverage in `tests/fuzz_headers.c`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-http2.h`](../include/mulib-http2.h) |
| Implementation switch | `MULIB_IMPL_HTTP2` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md) |
| Complete example | [`examples/modules/http2.c`](../examples/modules/http2.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/http2.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
