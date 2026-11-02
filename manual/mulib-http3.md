# HTTP/3 frame envelopes

Define `MULIB_IMPL_HTTP3`, `QUIC` and `BUFFER`. Reads/writes variable-integer
type/length envelopes only, preserving unknown frame types. Parse returns
1/0/-1/-2 for OK/MORE/invalid/limit. Payload is borrowed; failure unchanged.
No QPACK, stream-role validation, SETTINGS semantics or transport.
[Example](../examples/modules/http3.c), [limits](storage-server.md#http-and-policies).
