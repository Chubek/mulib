# Owned HTTP responses

Define `MULIB_IMPL_HTTP_RESPONSE`, `WEBFRAMEWORK` and its HTTP closure. Reuses
the existing bounded validated reply builder, including its status/body rules.
Inputs are borrowed; output owns memory only on OK. Free with
`mulib_http_message_free`. [Example](../examples/modules/http_response.c),
[contracts](storage-server.md#http-and-policies).
