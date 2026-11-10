# lattice — implication closure

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_LATTICE`.

Zero-initialize then set `attributes` to 1–64. Add premise/conclusion bitmask
rules; out-of-universe bits, invalid state and allocation failure return 0,
success 1. Empty premises are unconditional; duplicate rules are allowed.
Closure repeatedly applies rules until fixed point, masking the input universe.
Join closes the union; meet intersects the closures of both inputs. Invalid
contexts yield zero. At most 64 expanding rounds occur; cost is O(64 × rules),
storage O(rules). Free releases rules and clears the configured attribute count.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-lattice.h`](../include/mulib-lattice.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Finite 64-attribute implication closure lattice with join/meet of closed sets.
 * C11 single-header; define MULIB_IMPL_LATTICE once. */
#ifndef MULIB_LATTICE_H
#define MULIB_LATTICE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t premise, conclusion;
  } mulib_lattice_rule_t;
  typedef struct
  {
    size_t attributes, count, capacity;
    mulib_lattice_rule_t *rules;
  } mulib_lattice_t;
  int mulib_lattice_add_rule (mulib_lattice_t *l, uint64_t premise,
                              uint64_t conclusion);
  uint64_t mulib_lattice_closure (const mulib_lattice_t *l, uint64_t attributes);
  uint64_t mulib_lattice_join (const mulib_lattice_t *l, uint64_t a, uint64_t b);
  uint64_t mulib_lattice_meet (const mulib_lattice_t *l, uint64_t a, uint64_t b);
  void mulib_lattice_free (mulib_lattice_t *l);
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
| Header | [`include/mulib-lattice.h`](../include/mulib-lattice.h) |
| Implementation switch | `MULIB_IMPL_LATTICE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/lattice.c`](../examples/modules/lattice.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/lattice.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
