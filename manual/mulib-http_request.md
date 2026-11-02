# Owned HTTP requests

Define `MULIB_IMPL_HTTP_REQUEST` and the [HTTP](mulib-http.md) closure. Inputs
are borrowed; construction serializes and reparses through shared validation.
On OK output owns fields/body; free with `mulib_http_message_free`. Failure
leaves output unchanged. [Example](../examples/modules/http_request.c),
[contracts](storage-server.md#http-and-policies).
