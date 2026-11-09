# mulib-eda-incremental

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-incremental.h`  
**Implementation switch:** `MULIB_IMPL_EDA_INCREMENTAL`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Dense adjacency invalidation propagation including cyclic dependencies; intended for small incremental graphs (O(n²) memory), not the final million-node build graph.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

n*n bytes for adjacency matrix; caller should use a sparse DAG representation for large dependency networks.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_INCREMENTAL` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_INCREMENTAL
#include "mulib-eda-incremental.h"
```

## Focused regression test

`tests/test_eda_incremental.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_incremental.c -lm -o /tmp/test_eda_incremental
/tmp/test_eda_incremental
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-incremental.h`](../include/mulib-eda-incremental.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-incremental.h
 * @brief Dense adjacency invalidation propagation including cyclic dependencies; intended for small
 * incremental graphs (O(n²) memory), not the final million-node build graph.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_INCREMENTAL in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_INCREMENTAL_H
#define MULIB_EDA_INCREMENTAL_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_incremental {
    uint8_t *edge, *dirty;
    size_t count;
} mulib_eda_incremental_t;
int mulib_eda_incremental_init(mulib_eda_incremental_t *g, size_t nodes);
int mulib_eda_incremental_depend(mulib_eda_incremental_t *g, size_t prerequisite, size_t dependent);
int mulib_eda_incremental_invalidate(mulib_eda_incremental_t *g, size_t changed);
void mulib_eda_incremental_clear(mulib_eda_incremental_t *g);
void mulib_eda_incremental_free(mulib_eda_incremental_t *g);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_INCREMENTAL_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-incremental.h`](../include/mulib-eda-incremental.h) |
| Implementation switch | `MULIB_IMPL_EDA_INCREMENTAL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-incremental.c`](../examples/modules/eda-incremental.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-incremental.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
