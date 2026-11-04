# Owned HTTP responses

Define `MULIB_IMPL_HTTP_RESPONSE`, `WEBFRAMEWORK` and its HTTP closure. Reuses
the existing bounded validated reply builder, including its status/body rules.
Inputs are borrowed; output owns memory only on OK. Free with
`mulib_http_message_free`. [Example](../examples/modules/http_response.c),
[contracts](storage-server.md#http-and-policies).

Status must be 100..599. Bodies are rejected for 1xx, 204, 205 and 304; a 205
response is emitted with explicit zero-length framing. Content-Type is optional
and validated against header injection. Nonempty body data must have a valid
pointer and is copied byte-for-byte, including embedded NUL. The helper supplies
a reason phrase (a generic phrase for statuses without a dedicated mapping).
The result must be empty/unowned before success and freed before reuse.

This convenience constructor does not know whether the request was HEAD or
CONNECT. For those contexts, custom headers/reason phrases or nondefault limits,
use the shared HTTP message and serializer with the appropriate response flags.
Failure—including invalid status/body, oversized output and allocation failure—
leaves the output unchanged. The HTTP hardening and allocation suites verify this.
