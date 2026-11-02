# Token bucket

Define `MULIB_IMPL_RATE_LIMIT`. Initialize finite rate >=0, burst >0 and
nonnegative monotonic seconds. Take requires a finite positive cost. Denial can
update refill state; invalid/backward timestamps grant nothing and change
nothing. Caller owns synchronization. [Example](../examples/modules/rate_limit.c),
[contracts](storage-server.md#http-and-policies).
