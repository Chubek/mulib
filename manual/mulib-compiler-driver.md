# MuLIB compiler-driver

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/compiler-driver.c)

`add` appends a named callback and context. Callback/name must be non-NULL;
the descriptor is copied, while name and context remain borrowed. `run` requires
a non-NULL program, calls stages in insertion order, and stops at the first zero
callback result. `failed_index` receives that index, or stage count on success.
An empty pipeline succeeds. Argument failure leaves the index unchanged.

Earlier stages and the failing stage may have modified the program; there is no
rollback. Callbacks must not mutate the stage list while running. Zero-initialize
the driver; `free` releases descriptors, not names, contexts, or program data.
Append is amortized O(1), dispatch O(stages) plus callback costs.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-compiler-driver.h`](../include/mulib-compiler-driver.h) |
| Implementation switch | `MULIB_IMPL_COMPILER_DRIVER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/compiler-driver.c`](../examples/modules/compiler-driver.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/compiler-driver.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
