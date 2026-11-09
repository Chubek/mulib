# mulib-eda-hierarchy

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-hierarchy.h`  
**Implementation switch:** `MULIB_IMPL_EDA_HIERARCHY`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Append-only hierarchy with unique sibling names and slash-separated paths. Path construction supports any depth allowed by the index type and available storage, detects cycles and preserves the destination on failure. Parameterized elaboration is outside this profile.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Uses C11 size_t and fixed-width integers as documented in the public API. Calculations using double follow IEEE-754 semantics where supported; no bitwise cross-platform floating-point reproducibility is claimed.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_HIERARCHY` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_HIERARCHY
#include "mulib-eda-hierarchy.h"
```

## Focused regression test

`tests/test_eda_hierarchy.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_hierarchy.c -lm -o /tmp/test_eda_hierarchy
/tmp/test_eda_hierarchy
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-hierarchy.h`](../include/mulib-eda-hierarchy.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-hierarchy.h
 * @brief Append-only hierarchy with unique sibling names and slash-separated paths.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_HIERARCHY in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_HIERARCHY_H
#define MULIB_EDA_HIERARCHY_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_hnode {
    uint32_t parent;
    char name[48];
} mulib_eda_hnode_t;
typedef struct mulib_eda_hierarchy {
    mulib_eda_hnode_t *nodes;
    size_t count, capacity;
} mulib_eda_hierarchy_t;
int mulib_eda_hierarchy_add(mulib_eda_hierarchy_t *h, uint32_t parent, const char *name,
                            uint32_t *out);
int mulib_eda_hierarchy_path(const mulib_eda_hierarchy_t *h, uint32_t id, char *dst, size_t len);
void mulib_eda_hierarchy_free(mulib_eda_hierarchy_t *h);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_HIERARCHY_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-hierarchy.h`](../include/mulib-eda-hierarchy.h) |
| Implementation switch | `MULIB_IMPL_EDA_HIERARCHY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-hierarchy.c`](../examples/modules/eda-hierarchy.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-hierarchy.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
