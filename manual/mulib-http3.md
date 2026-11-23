# HTTP/3 frames and streams

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_HTTP3`, `MULIB_IMPL_QUIC` and `MULIB_IMPL_BUFFER`. The codec
handles QUIC-varint type/length envelopes and validates frame-local HTTP/3 payload
structure. It also provides an HTTP/3 control/request stream validator and a
literal-only QPACK field-section codec. Supply ordered, authenticated QUIC
stream bytes from a separate transport, or enable the live client below.

`mulib_http3_frame_read(data, length, max_payload, &frame, &consumed)` returns
1 OK, 0 MORE, -1 invalid, -2 limit, or **-3 allocation failure**. Both output
arguments are unchanged on every non-success. The payload borrows input.
`max_payload` is an explicit cap; zero permits only empty payloads. Limits are
checked as soon as the complete envelope is available, before waiting for the
advertised payload. Only a complete outer frame is semantically validated: a
truncated identifier *inside* that frame is INVALID, not MORE.

`mulib_http3_frame_write(&buffer, &frame)` appends transactionally and returns
1 success, 0 invalid/allocation failure. Initialize the buffer first. Payload
may alias its existing bytes. Type/length must fit QUIC's 62-bit integer range.
The envelope is written using minimal varints; payload bytes are retained.

## Payload validation

* HTTP/2-reserved frame types 0x2, 0x6, 0x8 and 0x9 are invalid in HTTP/3.
* CANCEL_PUSH (0x3), GOAWAY (0x7) and MAX_PUSH_ID (0xd) contain exactly one
  complete varint, without trailing bytes. PUSH_PROMISE (0x5) begins with one
  complete push-ID varint; the remaining QPACK bytes are opaque.
* SETTINGS (0x4) contains complete identifier/value varint pairs. Empty SETTINGS
  is valid. Duplicate identifiers are rejected, including unknown identifiers
  and identical integers represented with different varint widths.
* HTTP/2-reserved SETTINGS identifiers 0x2 through 0x5 are rejected.
  SETTINGS_ENABLE_CONNECT_PROTOCOL (0x8) and H3_DATAGRAM (0x33) must be 0 or 1.
  Unknown setting identifiers and their values remain valid.
* DATA, HEADERS and unknown extension/grease frame payloads remain opaque.
  Nonminimal QUIC varints are accepted on input, including in SETTINGS.

SETTINGS is first checked for well-formed pairs without allocating. Up to 32
identifiers fit in a stack array; larger frames allocate exactly eight bytes
per identifier, with overflow checks. Sorting detects duplicates in O(n log n)
work rather than a quadratic scan. Allocation failure is reported as -3 by the
reader and 0 by the writer, with no published output or leaked temporary array.

## Stream state and QPACK

Initialize separate state for each control stream or request half. Consume the
control stream type before feeding frames. `mulib_http3_stream_next` requires
SETTINGS first on the control stream, rejects duplicate SETTINGS and misplaced
frames, and checks the direction and monotonicity of GOAWAY. A request half
admits initial HEADERS, DATA, then optional trailers. Protocol errors latch an
HTTP/3 application error; incomplete input or a local payload/allocation limit
leaves the state unchanged. Call `mulib_http3_stream_finish` on FIN and indicate
whether trailing partial frame bytes remain.

The QPACK codec writes and reads field sections with zero dynamic insert count,
literal names, and non-Huffman strings. It preserves the never-index flag.
Field views borrow the caller's input and require a separate output array.
Callers validate HTTP field semantics and set frame/field budgets. Indexed field
representations and Huffman strings return -4 in the *literal codec*.

## Native HTTP/3 GET

Define `MULIB_IMPL_HTTP3_CLIENT` alongside `MULIB_IMPL_HTTP3`, the
[native QUIC client switches](mulib-quic.md#native-quic-v1-client), and their
dependencies in one C11 implementation translation unit:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude examples/http3-get.c -o http3-get
./http3-get 'https://host.example/path?q=1' [CA.pem]
```

`mulib_http3_get(url, options, &status, &body)` uses ALPN `h3`, verifies the
server certificate, and sends one GET with no request body. The `https` URL
may include a port, path and query; fragments are never sent. The call publishes
the final status and replaces the caller's initialized body buffer only after
a complete response and peer SETTINGS. Read the body as binary and release it
with `mulib_buffer_free`. The `MULIB_QUIC_*` result codes distinguish timeouts,
I/O, bounds, authentication and protocol failures. All HTTP statuses, including
non-2xx, are returned as successful HTTP exchanges when the message is valid.

The live client opens the HTTP/3 control and QPACK streams, validates SETTINGS
and critical streams, accepts informational responses and trailers, and checks
the response body against Content-Length if present. Its QPACK decoder accepts
static indexes, literal names or static name references, and HPACK Huffman
strings. Dynamic QPACK is disabled in SETTINGS; each decoded field section is
bounded at 64 KiB. Response headers are validated but not exposed. There is no
HTTP/3 server, persistent request pool, push, CONNECT, redirect follow-up,
decompression, or fallback to TCP. The native QUIC profile's limits also apply.

To run the independent loopback test, install Python `aioquic` and the
`openssl` command, configure with `-DMULIB_BUILD_HTTP3_INTEROP_TESTS=ON`,
build `mulib_test_http3_client`, and run
`ctest --test-dir BUILD -R http3 --output-on-failure`. Neither dependency is
used by the MuLIB client.

[Example](../examples/modules/http3.c). Regression and allocation suites:
`tests/test_http_hardening.c`, `tests/test_http_alloc.c`, and
`tests/test_quic_http3_stream.c`. The shared fuzz target
also checks parse/write/read semantics and failure output preservation.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-http3.h`](../include/mulib-http3.h) |
| Implementation switch | `MULIB_IMPL_HTTP3` |
| Repository-local includes | [`include/detail/mulib-http3-client.h`](../include/detail/mulib-http3-client.h), [mulib-quic](mulib-quic.md) |
| Complete example | [`examples/modules/http3.c`](../examples/modules/http3.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/http3.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
