# AsOf — Knowledge-time snapshots

Supply effective and known times separately in nonnegative Unix seconds.
An eligible revision must pass both cutoffs. The snapshot API requires
sorted unique keys, distinguishes present, missing and withdrawn entries,
and rejects conflicting winning revisions. A visible withdrawal does not
fall back to earlier values. Nothing here verifies ingestion timestamps
or persists the snapshot. See [FinPRED verification and limits](finpred.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-asof.h`](../include/mulib-asof.h) |
| Implementation switch | `MULIB_IMPL_ASOF` |
| Repository-local includes | None |
| Complete example | [`examples/hospitality_demo.c`](../examples/hospitality_demo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/hospitality_demo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
