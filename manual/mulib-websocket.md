# mulib-websocket

[Manuals](README.md) · [Library index](index.md)

Client opening-handshake and bidirectional RFC 6455 frame helpers. This is
not a connection implementation: supply your own transport (verified TLS
for `wss`), timeouts, peer policy, and connection close/control responses.

## Integration

Define `MULIB_IMPL_CDD_WEBSOCKET` in one C11 translation unit. Also enable
`MULIB_IMPL_BASE64`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_UTF8`, and the
[HTTP/URI dependencies](mulib-http.md) if not already linked. The declarations
have C++17 C linkage. Legacy `MULIB_IMPL_WEBSOCKET` and `mulib_websocket_*`
remain available. See the offline [`websocket.c` example](../examples/modules/websocket.c).

## Client handshake

1. Pass 16 fresh bytes from a cryptographically secure RNG to
   `mulib_cdd_websocket_key`. The key is 24 characters plus a NUL. The
   separate `mulib_cdd_websocket_accept` helper produces the 28-character
   RFC accept value plus a NUL; it is not authentication.
2. Build a request with `mulib_cdd_websocket_upgrade_request`. Supply an HTTP
   Host authority, including any nondefault port, an origin-form request
   target without a fragment, and at most one optional HTTP subprotocol
   token. No extensions are offered. The request is limited to 16384 bytes;
   `free` the returned string when done. NULL means invalid input, limit, or
   allocation failure.
3. Parse the response with `mulib_http_parse`. Validate it with
   `mulib_cdd_websocket_validate_upgrade` if no protocol was offered, or
   `mulib_cdd_websocket_validate_upgrade_protocol` with the original token.
   The selected protocol must match exactly if present; omission is allowed
   by RFC 6455 (check it separately if mandatory for your application). The
   validator requires an HTTP/1.1 101, `Upgrade: websocket`, an `Upgrade`
   token in `Connection`, and a single correct accept value. It rejects
   unsolicited extensions/protocols and ambiguous singleton fields. The
   legacy validator checks syntax but does not compare the returned protocol
   to the offer. Bytes after the HTTP parser's `consumed` boundary are
   WebSocket input, not part of the opening handshake.

## Frames and fragmentation

- `mulib_cdd_websocket_frame_write` atomically appends a frame to an
  initialized `mulib_buffer_t`. Maximum payload is 16 MiB. A client **must**
  set `masked=1` and supply a fresh, unpredictable four-byte mask for every
  frame, including control frames. A server **must** write unmasked frames.
- `mulib_cdd_websocket_frame_read` parses a single prefix. Set `expected_mask`
  to `0` when receiving from a server, `1` when receiving from a client.
  `-1` accepts either direction for offline decoding only. On `1`, consume
  exactly `consumed` bytes and release the owned frame with
  `mulib_cdd_websocket_frame_free`. On `0`, keep the prefix, append more
  bytes, and retry; `-1` is a protocol error, `-2` the payload limit
  (default 16 MiB), and `-3` allocation failure. Output and `consumed` are
  unchanged until success. Do not overwrite a frame that owns a payload.
- Both helpers reject invalid opcodes/reserved bits, noncanonical lengths
  (on read), fragmented/oversized control frames, invalid Close status or
  reason, and malformed UTF-8 in complete text frames. Close 1010 is
  client-only. Extension-defined close codes are not implemented.
- Track one zero-initialized `mulib_cdd_websocket_sequence_t` per direction
  and call `mulib_cdd_websocket_sequence_check` in frame order. It checks
  continuation order, UTF-8 across fragments, and a cumulative message
  payload limit (default 16 MiB). Control frames may interleave; a Close
  abandons an unfinished message and forbids further data in that
  direction. The checker does not reassemble messages or implement the
  two-sided closing handshake. Use the wire helpers for role masking and
  fail the connection on protocol errors. Applications that need whole
  messages should defer committing text data until the final frame validates.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-websocket.h`](../kernels/mulib-websocket.h) |
| Implementation switch | `MULIB_IMPL_CDD_WEBSOCKET` |
| Repository-local includes | [mulib-base64](mulib-base64.md), [mulib-buffer](mulib-buffer.md), [mulib-http](mulib-http.md), [mulib-uri](mulib-uri.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/websocket.c`](../examples/modules/websocket.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/websocket.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
