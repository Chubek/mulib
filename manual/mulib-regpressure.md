# MuLIB regpressure

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/regpressure.c)

`peak` returns the maximum number of simultaneously live inclusive intervals.
Each interval counts separately, even with duplicate value IDs. Intervals need
not be sorted; touching endpoints overlap. It tests every start position, where
an inclusive interval population can increase, so SIZE_MAX endpoints need no
end+1 arithmetic.

Invalid intervals or arguments return 0 without changing output. NULL ranges at
count zero succeed with peak 0. Time O(n²), extra storage O(1); this routine
measures pressure rather than choosing registers or building interference edges.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-regpressure.h`](../include/mulib-regpressure.h) |
| Implementation switch | `MULIB_IMPL_REGPRESSURE` |
| Repository-local includes | [mulib-liveintervals](mulib-liveintervals.md) |
| Complete example | [`examples/modules/regpressure.c`](../examples/modules/regpressure.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/regpressure.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
