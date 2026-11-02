# QUIC wire helpers

Define `MULIB_IMPL_QUIC` and `BUFFER`. RFC 9000 variable integers up to 2^62-1,
plus length-bearing STREAM frames with optional offset/FIN. Read returns 1 OK,
0 truncated, -1 invalid. Payload views borrow input; writes append atomically.
No packet protection, TLS, ACKs, congestion or network transport. Do not send
these frames as unauthenticated packets. [Example](../examples/modules/quic.c),
[limits](storage-server.md#http-and-policies).
