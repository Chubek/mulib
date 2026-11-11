# https — verified TLS plus HTTP/1.1

[Manuals](README.md) · [Library index](index.md)

Implementation switches: `MULIB_IMPL_STRBUF`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_PARSEC`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_URI`,
`MULIB_IMPL_HTTP`, `MULIB_IMPL_WEBCLIENT`, `MULIB_IMPL_TSL`, `MULIB_IMPL_HTTPS`.
See [`examples/modules/https.c`](../examples/modules/https.c).

For native cryptographic TLS, pass `mulib_tls_connect` and an optional
`mulib_tls_options_t` as the connector/context. Select its implementation
dependencies as described in [TLS 1.3](mulib-tls.md). [MuW3](mulib-muw3.md)
selects this connector by default.

`request` accepts an absolute https URL and the same method/header/body/options
model as webclient. Userinfo and fragments are rejected. The effective port is
443 when omitted. A caller connect callback receives the original HTTPS
endpoint and a zero-state `mulib_tsl_t` to initialize. It must configure hostname
and certificate-chain verification for that exact endpoint and HTTP/1.1 ALPN
or equivalent protocol selection. It returns 1 when initialization succeeds.
Explicit ports must be nonempty decimal values in 1..65535. Invalid arguments,
ports and response-context flags are rejected before connecting. Failure to
allocate the internal routing URI is NOMEM, rather than invalid input.
`options.http.flags` may contain `MULIB_HTTP_CHUNK_METADATA` to accept validated
extensions and separate trailer fields, as documented by the shared HTTP parser.

Every transaction creates a new TLS session, completes the verified handshake,
writes the full HTTP request with short-write handling, feeds response chunks
to webclient's bounded parser, then closes the session on all paths. HTTP Host,
target generation, request validation, informational responses and body framing
reuse existing webclient behavior. No plaintext fallback, redirect, pooled
connection or TLS library is implicit. The internal HTTP routing URI is only
for message construction; bytes are sent exclusively through TLS operations.

This API is synchronous: backend callbacks must block or fail; AGAIN aborts
the transaction. Backend code owns timeout/cancellation policy. Results and
response ownership are `mulib_webclient_result` and `mulib_http_message_t`;
response is unchanged on failure and must be freed on success. Adapter tests
cover failed verification, no-progress writes, short I/O and closure on every
path. `tests/test_tls_integration.py` additionally exercises real TLS 1.3 through
MuW3 against an independent local TLS server.

Backend EOF must represent authenticated TLS shutdown (`close_notify`), not an
unexplained underlying socket EOF. Report abrupt closure as a backend error so
close-delimited HTTP responses cannot silently accept truncated content. This
adapter deliberately requires successful transport EOF even if a Content-Length
frame appeared complete; a later TLS failure makes the transaction fail.

`mulib_https_stream_request` adds a `mulib_http_body_fn` callback/context before
the output argument, forwarding live decoded body slices without retaining them.
It otherwise uses the same verified TLS lifecycle, framing, cumulative limits and
ownership as `request` and `mulib_webclient_stream_request`. Successful output
owns headers/trailers only. Callbacks can precede a later framing or TLS failure;
the transaction is verified only when the API returns success.

`tests/test_http_hardening.c` additionally checks invalid ports, failed connect
after session initialization, handshake AGAIN, backend overreported write counts,
authentication failures, informational responses, raw response limits and exact
request construction. `tests/test_http_alloc.c` fails every transaction allocation
and checks unchanged response ownership and exactly-once session cleanup.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-https.h`](../include/mulib-https.h) |
| Implementation switch | `MULIB_IMPL_HTTPS` |
| Repository-local includes | [mulib-tsl](mulib-tsl.md), [mulib-webclient](mulib-webclient.md) |
| Complete example | [`examples/modules/https.c`](../examples/modules/https.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/https.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
