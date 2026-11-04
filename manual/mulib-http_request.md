# Owned HTTP requests

Define `MULIB_IMPL_HTTP_REQUEST` and the [HTTP](mulib-http.md) closure. Inputs
are borrowed; construction serializes and reparses through shared validation.
On OK output owns fields/body; free with `mulib_http_message_free`. Failure
leaves output unchanged. [Example](../examples/modules/http_request.c),
[contracts](storage-server.md#http-and-policies).

Default HTTP limits apply. The header array must include exactly one valid Host.
A supplied Transfer-Encoding header selects chunked encoding of the **decoded**
body bytes: the only accepted coding is `chunked` (case-insensitive with outer
whitespace trimmed by the validator). Unsupported/repeated transfer codings and
Content-Length/Transfer-Encoding combinations are invalid. Without transfer
coding, Content-Length is checked or generated. Binary bodies, including NUL,
are copied independently of the input. All source strings/bytes remain borrowed
only for the call; successful output must not overwrite an already owned message.

Use `mulib_http_serialize` on a message specification directly when custom limits
are needed. Tests exercise binary chunked construction and every allocation
failure in `tests/test_http_hardening.c` and `tests/test_http_alloc.c`.
