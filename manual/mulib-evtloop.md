# Cooperative event loop

Define `MULIB_IMPL_EVTLOOP`; no MuLIB dependencies. Zero-initialize and set a
nonzero task limit. Post absolute uint64 due ticks, cancel by id, run with
monotonic time and a callback budget. Due/id ordering is deterministic. Callbacks
may post/cancel but not free the loop. No OS I/O polling or threads.
[Example](../examples/modules/evtloop.c), [contracts](storage-server.md).
