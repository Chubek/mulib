# MuLIB compiler-test

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/compiler-test.c)

Zero-initialize `mulib_compiler_test_t`. `check` increments `passed` for a nonzero
condition and returns 1. Otherwise it increments `failed`, retains the borrowed
name in `last_failure`, and returns 0. A successful check does not clear the last
failure. Names must outlive their use through `last_failure`.

NULL arguments and counter overflow return 0 without incrementing counters.
`ok` returns true for a non-NULL harness with zero failures, including an empty
harness; it does not count invalid check calls as failures. No allocations or
printing occur. All operations take O(1) time.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-compiler-test.h`](../include/mulib-compiler-test.h) |
| Implementation switch | `MULIB_IMPL_COMPILER_TEST` |
| Repository-local includes | None |
| Complete example | [`examples/modules/compiler-test.c`](../examples/modules/compiler-test.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/compiler-test.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
