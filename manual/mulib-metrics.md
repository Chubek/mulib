# HTTP metrics

Define `MULIB_IMPL_METRICS`. Zero-initialized single-threaded counters record
requests, bytes, caller-supplied duration microseconds and status classes 1..5.
Counters saturate at UINT64_MAX. No global registry/exporter/synchronization.
[Example](../examples/modules/metrics.c), [contracts](storage-server.md).
