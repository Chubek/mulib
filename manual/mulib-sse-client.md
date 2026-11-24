# mulib-sse-client

[Manuals](README.md) · [Library index](index.md)

`mulib_cdd_sse_client_get` performs one synchronous streaming HTTP GET. It
sends `Accept: text/event-stream`, `Cache-Control: no-cache`, and
`Accept-Encoding: identity`. A nonempty `last_event_id` option adds
`Last-Event-ID`; the borrowed string must be valid UTF-8, no more than 8192
bytes, and have no HTTP control characters other than HTAB. This option is a
request header, not a seed for the response stream's `id` buffer.

## Integration

In one C11 translation unit, define `MULIB_IMPL_CDD_SSE_CLIENT` and enable
`MULIB_IMPL_SSE`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_STRBUF` and the
[MuW3 implementation and dependencies](mulib-muw3.md). Declarations have
C++17 C linkage. The legacy `MULIB_IMPL_SSE_CLIENT` switch and
`mulib_sse_client_get` function remain available. See the offline
[`sse-client.c` example](../examples/modules/sse-client.c).

## Delivery, bounds, and results

- Only status 200 is decoded as SSE. It must have exactly one
  `Content-Type: text/event-stream` with syntactically valid optional
  parameters, and absent or explicit `identity` content encoding. The
  charset parameter does not affect decoding; SSE is always UTF-8. Other
  HTTP statuses (including 204 and redirects) are returned without events.
- Arbitrary HTTP/chunk/line boundaries are supported. Ill-formed UTF-8 is
  replaced with U+FFFD. The SSE codec handles an initial BOM, comments,
  CR/LF/CRLF, multiline data, and `id` persistence within this response. An
  event without its terminating blank line is discarded. Event data is
  length-delimited and may contain NUL; the legacy C string representation
  cannot represent NUL in event names, so such names cause a decode error.
  `retry` has no effect; there is no automatic reconnect or redirect.
- The callback receives borrowed event strings; copy anything you need
  after it returns. Return exactly `1` to continue. Delivery is live and
  can precede an HTTP framing error, such as a truncated final chunk.
- `max_event_bytes` bounds decoded event and line processing (default
  1 MiB, see the SSE codec). MuW3's cumulative header/body and transport
  limits still apply; this is not an unbounded EventSource connection.
- `1` transfers response metadata, not a retained body, to `out`. Free it
  with `mulib_muw3_response_free`. `out` must not already own a response and
  is unchanged on failure. MuW3 errors pass through; `-2`/`-3` can also
  report an SSE limit or allocation failure. `MULIB_CDD_SSE_CLIENT_STOP`
  (`-7`) reports consumer stop, `MULIB_CDD_SSE_CLIENT_REPRESENTATION` (`-8`)
  disallowed MIME or encoding, and `MULIB_CDD_SSE_CLIENT_EVENT` (`-9`) an
  SSE decode error. The legacy function maps consumer stop and SSE parser
  errors to `-9`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-sse-client.h`](../kernels/mulib-sse-client.h) |
| Implementation switch | `MULIB_IMPL_CDD_SSE_CLIENT` |
| Repository-local includes | [mulib-muw3](mulib-muw3.md), [mulib-sse](mulib-sse.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/sse-client.c`](../examples/modules/sse-client.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/sse-client.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
