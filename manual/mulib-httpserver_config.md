# HTTP server limits

Define `MULIB_IMPL_HTTPSERVER_CONFIG`. The HTTP model is a declaration dependency.
Defaults specify header/body/buffer/connection limits and idle timeout. Validation
requires explicit limits and checks sum overflow. No listener, resource allocation
or timeout enforcement. [Example](../examples/modules/httpserver_config.c),
[contracts](storage-server.md#http-and-policies).

Header-array allocation must be representable (`max_headers * sizeof(header)`),
as must the aggregate buffer budget (`max_connections * max_buffer_bytes`).
Validation rejects zero connections/timeouts/limits and per-message EOF or
response-context flags. Checked arithmetic prevents wraparound in these budgets.

The buffer lower bound is headers plus **decoded** body bytes. Chunk framing,
buffered pipeline data and application metadata can require additional room;
choose max_buffer_bytes for the intended buffering/streaming policy. Validation
does not reserve memory or guarantee that every maximally chunked representation
fits a buffer at that lower bound. Actual allocation and connection admission
remain the server's responsibility.
