# Health hysteresis

Define `MULIB_IMPL_HEALTH_CHECKER`. The upstream type is a declaration dependency;
no upstream functions are called. Nonzero rise/fall thresholds and zero-initialized
counters apply externally supplied consecutive probe results. No active network
probes. [Example](../examples/modules/health_checker.c), [contracts](storage-server.md).
