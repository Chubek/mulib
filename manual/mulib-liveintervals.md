# MuLIB liveintervals

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/liveintervals.c)

A `mulib_liveinterval_t` contains an application value ID and inclusive start/end
instruction positions. `valid` accepts a non-NULL interval with start <= end;
`overlap` requires both valid and returns true when their closed ranges intersect.
Intervals touching at a single endpoint overlap, including zero-width intervals.

SIZE_MAX is permitted as a position or value ID; no endpoint arithmetic is used.
The value ID is metadata, not an array index imposed by these functions. There is
no ownership, allocation, sorting, or interval derivation; both predicates are O(1).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-liveintervals.h`](../include/mulib-liveintervals.h) |
| Implementation switch | `MULIB_IMPL_LIVEINTERVALS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/liveintervals.c`](../examples/modules/liveintervals.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/liveintervals.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
