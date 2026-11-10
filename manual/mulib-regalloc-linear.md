# MuLIB regalloc-linear

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/regalloc-linear.c)

`mulib_regalloc_linear` accepts up to 65536 inclusive intervals, in any order.
It stably sorts by start, expires intervals only when end < current start, and
selects the lowest free register bit. Under pressure it evicts an active interval
with a strictly later end than the current one; otherwise it spills the current
interval. Ties retain earlier assignments.

Locations are indexed by original input position; value IDs are metadata. Spills
use reg == -1 and unique stable slots, while register locations use spill ==
SIZE_MAX. Eviction changes the victim's final location for its whole interval;
the caller must emit corresponding loads/stores, not assume interval splitting.
Invalid input/allocation failure preserves outputs. Empty arrays succeed. Time
O(n²), scratch O(n); register mask may be zero.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-regalloc-linear.h`](../include/mulib-regalloc-linear.h) |
| Implementation switch | `MULIB_IMPL_REGALLOC_LINEAR` |
| Repository-local includes | [mulib-liveintervals](mulib-liveintervals.md), [mulib-physreg](mulib-physreg.md) |
| Complete example | [`examples/modules/regalloc-linear.c`](../examples/modules/regalloc-linear.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/regalloc-linear.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
