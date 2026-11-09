# mulib-eda-ids

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-ids.h`  
**Implementation switch:** `MULIB_IMPL_EDA_IDS`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Typed 64-bit IDs: 8-bit kind, 24-bit generation and 32-bit index plus one; stale generations are rejected. No persistent serialization protocol.

## Behavior and correctness contract

Zero-initialize owned containers before first use; call initialization functions only on empty objects, and release owned storage with the matching free function. Callers retain ownership of streams and borrowed arrays. Public pointer fields must refer to live storage large enough for their declared counts; validation checks logical fields, not allocation bounds. Check return values before using outputs.

## Limits and numerical assumptions

Maximum 2^32-1 slots, 2^24-1 generations per slot; permanently retires exhausted slots. Each pool is externally synchronized.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_IDS` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_IDS
#include "mulib-eda-ids.h"
```

## Focused regression test

`tests/test_eda_ids.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_ids.c -lm -o /tmp/test_eda_ids
/tmp/test_eda_ids
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-ids.h`](../include/mulib-eda-ids.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-ids.h
 * @brief Typed 64-bit IDs: 8-bit kind, 24-bit generation and 32-bit index plus one; stale
 * generations are rejected.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_IDS in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_IDS_H
#define MULIB_EDA_IDS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t mulib_eda_id_t;
typedef struct mulib_eda_id_pool {
    uint32_t *generation;
    uint8_t *active;
    uint8_t *kind;
    size_t length, capacity;
} mulib_eda_id_pool_t;
int mulib_eda_id_pool_init(mulib_eda_id_pool_t *p, size_t initial_capacity);
void mulib_eda_id_pool_free(mulib_eda_id_pool_t *p);
int mulib_eda_id_create(mulib_eda_id_pool_t *p, uint8_t kind, mulib_eda_id_t *out);
int mulib_eda_id_valid(const mulib_eda_id_pool_t *p, mulib_eda_id_t id, uint8_t kind);
int mulib_eda_id_remove(mulib_eda_id_pool_t *p, mulib_eda_id_t id);
uint32_t mulib_eda_id_index(mulib_eda_id_t id);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_IDS_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-ids.h`](../include/mulib-eda-ids.h) |
| Implementation switch | `MULIB_IMPL_EDA_IDS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/eda-ids.c`](../examples/modules/eda-ids.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-ids.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
