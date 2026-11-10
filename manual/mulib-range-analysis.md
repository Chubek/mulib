# MuLIB range-analysis

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/range-analysis.c)

Ranges contain inclusive signed-64 endpoints and `known`. `const` constructs a
singleton; `join` returns the smallest covering interval, not an intersection.
`add` and `sub` compute endpoint bounds; `mul` checks all four endpoint products
and takes their minimum/maximum. Exact INT64_MIN subtraction is supported when
the result fits, without negating INT64_MIN.

Unknown input, reversed known endpoints, or any possible endpoint overflow
produces an unknown result. When `known == 0`, ignore both endpoint fields.
This is conservative non-wrapping arithmetic: it does not clamp, infer congruences,
or retain disjoint ranges. Operations have O(1) time/space and no allocations.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-range-analysis.h`](../include/mulib-range-analysis.h) |
| Implementation switch | `MULIB_IMPL_RANGE_ANALYSIS` |
| Repository-local includes | [`include/detail/mulib-compiler-integer.h`](../include/detail/mulib-compiler-integer.h) |
| Complete example | [`examples/modules/range-analysis.c`](../examples/modules/range-analysis.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/range-analysis.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
