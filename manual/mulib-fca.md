# fca — binary formal contexts

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_FCA`.

Init accepts 0–64 objects and attributes, clearing the fixed 64-row context.
Init/set return 1 on success, 0 on invalid dimensions/indices without mutation.
Bit i denotes object/attribute i. Extent selects objects possessing all requested
attributes; intent intersects attributes of selected objects. Empty derivations
use the full opposite universe. Out-of-universe mask bits are ignored. Closure
is intent(extent(attributes)); invalid contexts return zero from derivations.
Operations allocate nothing and cost O(objects). Context mutation invalidates
previously derived concepts but not independently copied masks.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-fca.h`](../include/mulib-fca.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Formal Concept Analysis binary contexts up to 64 objects and 64 attributes.
 * C11 single-header; define MULIB_IMPL_FCA once. */
#ifndef MULIB_FCA_H
#define MULIB_FCA_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    size_t objects, attributes;
    uint64_t rows[64];
  } mulib_fca_t;
  int mulib_fca_init (mulib_fca_t *c, size_t objects, size_t attributes);
  int mulib_fca_set (mulib_fca_t *c, size_t object, size_t attribute, int present);
  uint64_t mulib_fca_extent (const mulib_fca_t *c, uint64_t attributes);
  uint64_t mulib_fca_intent (const mulib_fca_t *c, uint64_t objects);
  uint64_t mulib_fca_closure (const mulib_fca_t *c, uint64_t attributes);
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
| Header | [`include/mulib-fca.h`](../include/mulib-fca.h) |
| Implementation switch | `MULIB_IMPL_FCA` |
| Repository-local includes | None |
| Complete example | [`examples/modules/fca.c`](../examples/modules/fca.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fca.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
