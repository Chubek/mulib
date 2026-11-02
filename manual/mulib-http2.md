# HTTP/2 frame envelopes

Define `MULIB_IMPL_HTTP2` and `BUFFER`. Reads/writes nine-byte envelopes with
basic per-type lengths, stream-id and padding checks. Unknown types/flags are
preserved. Parse returns 1/0/-1/-2 for OK/MORE/invalid/limit. Payload is borrowed.
No HPACK, flow control, SETTINGS semantics or connection state machine.
[Example](../examples/modules/http2.c), [limits](storage-server.md#http-and-policies).
