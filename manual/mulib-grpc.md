# gRPC frames and binary gRPC-Web

[Manuals](README.md) · [Library index](index.md)

Include [`mulib-grpc.h`](../kernels/mulib-grpc.h). Define `MULIB_IMPL_CDD_GRPC`
in one C translation unit and link [MuW3](mulib-muw3.md) and
[buffer](mulib-buffer.md). The `MULIB_IMPL_GRPC` spelling, original typedefs
and `mulib_grpc_*` entry points remain available.

## Frame codec

`mulib_cdd_grpc_frame_read` reads one five-byte length prefix and payload.
It returns `1` on success, `0` for more input, `-1` for invalid input and
`-2` for an advertised size above the limit. A zero limit selects 16 MiB.
The frame borrows input and `consumed` (optional) reports the exact boundary;
both outputs are unchanged on non-success. The only data-frame flags are
`0` (uncompressed) and `1` (compressed). This codec does not decompress.

`mulib_cdd_grpc_frame_write` appends one frame to an initialized
`mulib_buffer_t`. It returns `1` on success and leaves the buffer unchanged
on failure. The wire length is at most `UINT32_MAX`; the old
`mulib_grpc_frame_write` also keeps its original 16 MiB cap.

## Unary HTTP client

`mulib_cdd_grpc_web_unary` POSTs one uncompressed message to a gRPC-Web
endpoint. Its response parser is also available as
`mulib_cdd_grpc_web_decode` for an already parsed `mulib_http_message_t`.
Both require HTTP 200 and exactly one binary `Content-Type`
(`application/grpc-web+proto` or `application/grpc-web`, case insensitive).
HTTP and gRPC encoding fields must be absent or `identity`.

- Success (`grpc_status == 0`) requires exactly one uncompressed data
  frame followed by one final uncompressed trailer frame.
- Nonzero gRPC status may omit the data frame. An empty body with one
  nonzero `Grpc-Status` HTTP header (and optional `Grpc-Message`) is accepted
  as a trailers-only response. Ordinary HTTP chunk trailers cannot stand in
  for a gRPC-Web trailer frame.
- Trailer fields are CRLF-separated, with or without a final CRLF. Status
  must be one decimal value in `0..16`, without leading zeros. At most one
  `grpc-message` is accepted; it is validated for printable ASCII and
  percent-triplet syntax and returned still percent-encoded. Other metadata
  is not exposed separately. Trailer blocks have an independent 16 KiB cap.
- The message bound applies to each unary request and reply. MuW3's HTTP body
  limit still applies to the whole HTTP body, including frame overhead; raise
  it explicitly if you need a message near the default 16 MiB boundary.

Return `1` means the exchange was valid, not necessarily successful at the
gRPC layer. Inspect `grpc_status`. `-1` is invalid or unsupported, `-2` is a
limit, `-3` is allocation failure; other MuW3 errors are passed through.
Output is unchanged on failure. Start with a zero-initialized reply and free
it with `mulib_cdd_grpc_web_reply_free` before reuse. Only the unary transport
populates `reply.http`. A NULL `reply.message` means no data frame; a valid
zero-length data frame has non-NULL `reply.message`.

This is **binary unary gRPC-Web over HTTP/1**, not native gRPC over HTTP/2.
No HPACK or HTTP/2 transport, streaming RPCs, gRPC-Web text/base64 mode,
compression or Protobuf serialization is included.

[Complete offline example](../examples/modules/grpc.c) ·
[Focused tests](../tests/test-api-grpc.c)

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Ikernels \
  examples/modules/grpc.c -pthread -lm -ldl -o grpc-example
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-grpc.h`](../kernels/mulib-grpc.h) |
| Implementation switch | `MULIB_IMPL_CDD_GRPC` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-muw3](mulib-muw3.md) |
| Complete example | [`examples/modules/grpc.c`](../examples/modules/grpc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/grpc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
