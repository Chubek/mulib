# semilattice — 64-bit subset algebra

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_SEMILATTICE`.

Join is bitwise union, meet bitwise intersection, and leq tests subset inclusion.
All uint64 masks are valid; zero is bottom and UINT64_MAX top. Operations are
pure, constant-time and allocation-free. Join is commutative, associative and
idempotent, making it suitable for combining grow-only flags. This module
provides algebra only; it does not store replicas or perform synchronization.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-semilattice.h`](../include/mulib-semilattice.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Grow-only 64-bit-set join/meet semilattice and partial-order tests. C11
 * single-header; define MULIB_IMPL_SEMILATTICE once. */
#ifndef MULIB_SEMILATTICE_H
#define MULIB_SEMILATTICE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  uint64_t mulib_semilattice_join (uint64_t a, uint64_t b);
  uint64_t mulib_semilattice_meet (uint64_t a, uint64_t b);
  int mulib_semilattice_leq (uint64_t a, uint64_t b);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-semilattice.h`](../include/mulib-semilattice.h) |
| Implementation switch | `MULIB_IMPL_SEMILATTICE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/semilattice.c`](../examples/modules/semilattice.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/semilattice.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
