# MuLIB compiler-cache

[Compiler integration](compiler-tools.md) · [Example](../examples/modules/compiler-cache.c)

`put` copies arbitrary bytes under a caller-chosen 64-bit fingerprint, replacing
an existing entry atomically. NULL data is valid for zero length; replacement may
read from the entry being replaced. `get` returns a borrowed byte pointer and
length, leaving outputs unchanged on a miss. An empty artifact is distinct from
a miss. The borrowed bytes survive until replacement or cache destruction.

The cache does not hash inputs, detect fingerprint collisions, or persist to disk.
Include source, options, target, and compiler version in application keys.
Zero-initialize the cache and call `free` to release all copies. Lookup is O(n);
insertion/replacement is O(n + artifact length).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-compiler-cache.h`](../include/mulib-compiler-cache.h) |
| Implementation switch | `MULIB_IMPL_COMPILER_CACHE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/compiler-cache.c`](../examples/modules/compiler-cache.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/compiler-cache.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
