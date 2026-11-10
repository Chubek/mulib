# mulib-eda-ids

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-ids.h`  
**Implementation switch:** `MULIB_IMPL_EDA_IDS`  
**Dependencies:** C11 standard library only.

## Purpose and supported profile

Typed 64-bit IDs: 8-bit kind, 24-bit generation and 32-bit index plus one; stale generations are rejected. No persistent serialization protocol.

## Behavior and correctness contract

`mulib_eda_id_pool_init` accepts fresh, even uninitialized, struct storage.
It returns 1 on success, owning three arrays, or 0 for invalid arguments,
capacity overflow or allocation failure. All failures preserve the destination
byte-for-byte and release temporary allocations. Capacity zero allocates nothing.
Initialization is not replacement: free a live pool before initializing it again.
`mulib_eda_id_pool_free` releases the arrays and zeroes the struct; NULL and repeated
free are allowed. Never shallow-copy a live pool into another owner.

`create` requires a nonzero 8-bit kind and disjoint output storage; it returns 1
and writes an ID on success, or 0 without changing the pool/output on failure.
The first reusable slot is selected. Generations **1 through 0xffffff inclusive**
are usable. Removing generation 0xffffff leaves the internal counter at
0x1000000 and permanently retires that slot. Generation bits never wrap into a
previous ID. `remove` returns 0 without mutation for stale or invalid IDs.

`valid` checks slot bounds, activity, generation and encoded kind. Its kind
argument is either a required kind or zero (any kind). `index` merely decodes
the low 32 bits; UINT32_MAX means a zero low word, not a validity check.
IDs are local to one pool lifetime: independent/reinitialized pools may emit the
same numeric ID. Store a pool's identity separately when crossing that boundary.
Public array fields must describe valid owned storage; arbitrary forged pointers
and capacities are outside the API contract. All shared access requires external
synchronization, including readers during mutation.

## Limits and numerical assumptions

Maximum 2^32-1 slots, additionally limited by size_t allocation arithmetic and
available memory; 2^24-1 generations per slot. Initialization is O(capacity) and
uses three zeroed arrays (one uint32_t and two uint8_t elements per slot).
Creation scans O(length); growth copies O(length) into new arrays before publishing
them. Validation/removal are allocation-free O(1). There is no persisted ID format,
cross-pool uniqueness, database referential-integrity enforcement or free-list index.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_IDS` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_IDS
#include "mulib-eda-ids.h"
```

## Focused regression test

`tests/test_eda_ids.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

The suite checks final-generation issuance/retirement, stale and wrong-kind IDs,
empty initialization, and 4,096 deterministic create/remove operations against a
live/stale handle model. `tests/test_eda_alloc.c` fails each of the three allocations
in initialization and growth, checking cleanup, unchanged destinations and retry.
The boundary fixture seeds the generation counter near exhaustion; it does not
benchmark sixteen million cycles. The runnable example connects IDs to scalar
DB transactions and demonstrates that pool validation must precede DB lookup.

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
/** Initialize fresh (possibly uninitialized) storage with capacity in slots.
 * Returns 1 on success, 0 on NULL, capacity overflow or allocation failure.
 * Failure leaves *p byte-for-byte unchanged and frees temporary allocations.
 * Success owns three arrays; release with mulib_eda_id_pool_free. Do not
 * initialize over a live pool: successful initialization does not free it.
 * O(initial_capacity) time/storage; no global state or internal locking.
 */
int mulib_eda_id_pool_init(mulib_eda_id_pool_t *p, size_t initial_capacity);
/** Free owned arrays and zero the pool. NULL and repeated free are harmless.
 * Invalidates all IDs; IDs are pool-local and must not outlive this lifetime.
 */
void mulib_eda_id_pool_free(mulib_eda_id_pool_t *p);
/** Create a nonzero-kind ID in an initialized pool; return 1 on success.
 * Uses the first inactive, unexhausted slot, growing arrays if necessary.
 * Generations 1..0xffffff are usable; a slot is retired after removing its
 * final generation. Returns 0 on invalid arguments, slot/size limit or OOM,
 * leaving pool and *out unchanged. out must not alias pool-owned storage.
 * O(length) scan, O(capacity) on growth. Externally synchronize pool access.
 */
int mulib_eda_id_create(mulib_eda_id_pool_t *p, uint8_t kind, mulib_eda_id_t *out);
/** Return 1 only for a live matching ID in this pool; kind=0 accepts any kind.
 * O(1), allocation-free. NULL pool, zero ID and stale/wrong-kind IDs return 0.
 */
int mulib_eda_id_valid(const mulib_eda_id_pool_t *p, mulib_eda_id_t id, uint8_t kind);
/** Invalidate a live ID, advancing its generation without wrapping; return 1.
 * Invalid/stale IDs return 0 without mutation. O(1), allocation-free.
 */
int mulib_eda_id_remove(mulib_eda_id_pool_t *p, mulib_eda_id_t id);
/** Decode the zero-based slot, or UINT32_MAX for a zero low word.
 * This does not validate pool membership, kind or generation.
 */
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
