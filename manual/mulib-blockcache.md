# blockcache

[Manuals](README.md) · [Library index](index.md)

Initialize with nonzero entry/byte limits. `put` copies a uint64-keyed block;
oversized/invalid/allocation failures return 0 without eviction. On success it
replaces the existing key and evicts least-recently used blocks until both limits
hold. Inputs may alias cached bytes. Zero-length blocks still count as entries.

`get` promotes an entry and borrows bytes until the next mutation/free. Returns
1 hit, 0 absent/invalid; outputs unchanged on miss. No pinning, locking, file I/O
or thread safety. Release/reset with `free`. Lookup/promotion is linear.
Switch: `MULIB_IMPL_BLOCKCACHE`. Example: `examples/modules/blockcache.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-blockcache.h`](../include/mulib-blockcache.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-blockcache.h
 * @brief Bounded, owning LRU cache for binary blocks keyed by uint64_t.
 * Define MULIB_IMPL_BLOCKCACHE once. No locking, pinning or implicit I/O. */
#ifndef MULIB_BLOCKCACHE_H
#define MULIB_BLOCKCACHE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_blockcache_entry
  {
    uint64_t key;
    unsigned char *data;
    size_t size;
  } mulib_blockcache_entry_t;
  typedef struct mulib_blockcache
  {
    mulib_blockcache_entry_t *entries;
    size_t count, bytes, max_entries, max_bytes;
  } mulib_blockcache_t;
  /** @brief Initialize with nonzero entry and byte limits, no allocation. 1
   * succeeds. */
  int mulib_blockcache_init (mulib_blockcache_t *c, size_t entries,
                             size_t bytes);
  /** @brief Copy/replace, evicting least-recently used blocks only after
   * allocation. 1 succeeds, 0 invalid/oversize/OOM. Input may alias cached
   * data. */
  int mulib_blockcache_put (mulib_blockcache_t *c, uint64_t key,
                            const void *data, size_t size);
  /** @brief Promote and borrow until next mutation/free; 1 hit, 0 miss. */
  int mulib_blockcache_get (mulib_blockcache_t *c, uint64_t key,
                            const void **data, size_t *size);
  /** @brief Free/reset, accepting NULL. */
  void mulib_blockcache_free (mulib_blockcache_t *c);
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
| Header | [`include/mulib-blockcache.h`](../include/mulib-blockcache.h) |
| Implementation switch | `MULIB_IMPL_BLOCKCACHE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/blockcache.c`](../examples/modules/blockcache.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/blockcache.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
