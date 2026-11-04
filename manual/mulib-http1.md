# HTTP/1.1

Define `MULIB_IMPL_HTTP1` and the [HTTP](mulib-http.md) closure. The entry point
forwards to the existing strict parser, preserving limits, ownership, MORE/EOF
handling and framing rules. It is not a second parser or socket implementation.
[Example](../examples/modules/http1.c), [contracts](storage-server.md).

## Serialization and ownership

`mulib_http1_serialize(message, options, &bytes, &length)` is the symmetric
HTTP/1.1 serialization entry point. It uses exactly `mulib_http_serialize`'s
validation, aggregate resource limits, binary handling and response-context
rules. On success bytes is malloc-owned and has an extra NUL outside length;
free it with `free`. On failure both output arguments remain unchanged.

`mulib_http1_parse` transfers an owned message only on OK; failure leaves the
message unchanged and resets an optional consumed output to zero. Do not reuse
an owned message without freeing it first. HEAD/CONNECT context flags are
response-only and mutually exclusive. See [HTTP](mulib-http.md) for framing,
status-205 handling, EOF and pipeline boundaries.
