# Load balancing

Define `MULIB_IMPL_LOAD_BALANCER` and `UPSTREAM`. Round-robin or least-active
selection over a borrowed array, using round-robin tie breaking. Returns index
or SIZE_MAX. Selection does not acquire; caller synchronizes and balances
acquire/release. [Example](../examples/modules/load_balancer.c),
[contracts](storage-server.md#http-and-policies).
