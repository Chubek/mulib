# https — verified TLS plus HTTP/1.1

Implementation switches: `MULIB_IMPL_STRBUF`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_PARSEC`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_URI`,
`MULIB_IMPL_HTTP`, `MULIB_IMPL_WEBCLIENT`, `MULIB_IMPL_TSL`, `MULIB_IMPL_HTTPS`.
See [`examples/modules/https.c`](../examples/modules/https.c).

`request` accepts an absolute https URL and the same method/header/body/options
model as webclient. Userinfo and fragments are rejected. The effective port is
443 when omitted. A caller connect callback receives the original HTTPS
endpoint and a zero-state `mulib_tsl_t` to initialize. It must configure hostname
and certificate-chain verification for that exact endpoint and HTTP/1.1 ALPN
or equivalent protocol selection. It returns 1 when initialization succeeds.

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
response is unchanged on failure and must be freed on success. Tests use TLS
interface fixtures, not a real cryptographic implementation, and cover failed
verification, no-progress writes, short I/O and closure on every path.
