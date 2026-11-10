# MuLIB passmanager

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/passmanager.c)

`add` appends a callback, borrowed context, and preserved-analysis bitmask. `run`
requires a non-NULL program and invokes passes in order. After each invoked pass,
including a failing pass, `valid_analyses &= preserves`; a failed pass may already
have changed the program. No analyses are recomputed or marked valid implicitly.

Zero callback result stops execution. Optional `failed_index` is the failing
index or pass count on success. Empty pipelines succeed; invalid call arguments
leave that output unchanged. Program effects are retained, not rolled back.
Callbacks must not modify the pipeline during execution. Zero-initialize and
`free` after use; contexts/program remain caller-owned. Dispatch is O(passes).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-passmanager.h`](../include/mulib-passmanager.h) |
| Implementation switch | `MULIB_IMPL_PASSMANAGER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/passmanager.c`](../examples/modules/passmanager.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/passmanager.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
