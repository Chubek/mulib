# mulib-eda-undo

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-undo.h`  
**Implementation switch:** `MULIB_IMPL_EDA_UNDO`  
**Dependencies:** `eda-design-db`.

## Purpose and supported profile

Bounded 256-command reversible scalar-field journal over eda-design-db. Refuses history replay if an external edit changes the expected value. Not a universal undo journal for geometry or object allocation.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_UNDO` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_UNDO
#include "mulib-eda-undo.h"
```

## Focused regression test

`tests/test_eda_undo.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_undo.c -lm -o /tmp/test_eda_undo
/tmp/test_eda_undo
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-undo.h`](../include/mulib-eda-undo.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-undo.h
 * @brief Bounded 256-command reversible scalar-field journal over eda-design-db.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_UNDO in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_UNDO_H
#define MULIB_EDA_UNDO_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-eda-design-db.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_undo_entry {
    uint64_t id;
    double before, after;
} mulib_eda_undo_entry_t;
typedef struct mulib_eda_undo {
    mulib_eda_undo_entry_t entries[256];
    size_t count, cursor;
} mulib_eda_undo_t;
int mulib_eda_undo_apply(mulib_eda_undo_t *u, mulib_eda_design_db_t *db, uint64_t id, double after);
int mulib_eda_undo_back(mulib_eda_undo_t *u, mulib_eda_design_db_t *db);
int mulib_eda_undo_forward(mulib_eda_undo_t *u, mulib_eda_design_db_t *db);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_UNDO_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-undo.h`](../include/mulib-eda-undo.h) |
| Implementation switch | `MULIB_IMPL_EDA_UNDO` |
| Repository-local includes | [mulib-eda-design-db](mulib-eda-design-db.md) |
| Complete example | [`examples/modules/eda-undo.c`](../examples/modules/eda-undo.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-undo.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
