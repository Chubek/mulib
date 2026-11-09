# mulib-eda-design-db

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-design-db.h`  
**Implementation switch:** `MULIB_IMPL_EDA_DESIGN_DB`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Flat revisioned design store keyed by nonzero external IDs, with typed finite scalar values and independent owned snapshot cloning. Transactions are provided by eda-transaction. No persistent serialization, locking or hierarchical object graph.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_DESIGN_DB` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_DESIGN_DB
#include "mulib-eda-design-db.h"
```

## Focused regression test

`tests/test_eda_design_db.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_design_db.c -lm -o /tmp/test_eda_design_db
/tmp/test_eda_design_db
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-design-db.h`](../include/mulib-eda-design-db.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-design-db.h
 * @brief Flat revisioned design store keyed by nonzero external IDs, with typed finite scalar
 * values and independent owned snapshot cloning.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_DESIGN_DB in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_DESIGN_DB_H
#define MULIB_EDA_DESIGN_DB_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_db_record {
    uint64_t id;
    uint32_t type;
    double value;
} mulib_eda_db_record_t;
typedef struct mulib_eda_design_db {
    mulib_eda_db_record_t *items;
    size_t count, capacity;
    uint64_t epoch;
} mulib_eda_design_db_t;
int mulib_eda_db_put(mulib_eda_design_db_t *db, mulib_eda_db_record_t record);
int mulib_eda_db_get(const mulib_eda_design_db_t *db, uint64_t id, mulib_eda_db_record_t *out);
int mulib_eda_db_delete(mulib_eda_design_db_t *db, uint64_t id);
void mulib_eda_db_free(mulib_eda_design_db_t *db);
/* Clone into an empty destination. On failure destination is unchanged. */
int mulib_eda_db_clone(const mulib_eda_design_db_t *source, mulib_eda_design_db_t *destination);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_DESIGN_DB_H */
```

</details>
<!-- /manual-interface -->

Constructors and conversions require an empty destination; release existing owned storage before replacing it. Public pointer fields must refer to live arrays of the declared sizes. Validation checks logical structure, not the allocation bounds of caller-provided pointers.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-design-db.h`](../include/mulib-eda-design-db.h) |
| Implementation switch | `MULIB_IMPL_EDA_DESIGN_DB` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-design-db.c`](../examples/modules/eda-design-db.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-design-db.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
