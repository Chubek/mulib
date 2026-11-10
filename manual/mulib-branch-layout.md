# MuLIB branch-layout

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/branch-layout.c)

`mulib_branch_layout` relaxes instruction-index branches to a fixed point. Supply
one fixed length and one descriptor per instruction; `target == SIZE_MAX` selects
the fixed length. Other targets must be inside the array, short sizes nonzero,
and long sizes at least the corresponding short sizes. `conditional` is metadata;
the caller supplies the correct sizes for conditional or unconditional encodings.

All branches start short. Displacements are measured from the instruction end;
short branches accept -128 through 127, long branches signed 32-bit displacements.
Widening is monotone and repeated until stable. This computes offsets and form
flags, not machine bytes. Count must be 1..65536. Overflow, unrepresentable long
displacements, invalid input, and allocation failure return 0 without modifying
outputs. Worst-case time O(n²), scratch O(n).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-branch-layout.h`](../include/mulib-branch-layout.h) |
| Implementation switch | `MULIB_IMPL_BRANCH_LAYOUT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/branch-layout.c`](../examples/modules/branch-layout.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/branch-layout.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
