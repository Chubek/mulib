# memtable

[Manuals](README.md) · [Library index](index.md)

Owned binary keys/values in unsigned-byte lexicographic order. Initialize
`mulib_memtable_t` to zero. `put` copies inputs (including aliases into the table),
stores a uint64 sequence and a tombstone flag, and replaces only equal/newer
sequences. Tombstone values must be empty. Empty keys/values are valid.

`get` borrows a record, including deleted records; 0 found, 1 absent, negative
error. `lower_bound` returns the first key >= input. Inserts are O(n), lookups
O(log n). Mutation invalidates borrowed records. `clone` constructs a deep copy
in an uninitialized destination. `free` releases/reset storage. Failed writes
preserve logical state; revision increases only on successful mutation.

Switch: `MULIB_IMPL_MEMTABLE`. Example: `examples/modules/memtable.c`.
See [storage contracts and limits](extension-layers.md#database-storage).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-memtable.h`](../include/mulib-memtable.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-memtable.h
 * @brief Owned sorted binary records with sequence numbers and tombstones.
 * Define MULIB_IMPL_MEMTABLE once. Sorted-vector insertion is O(n), lookup
 * O(log n).
 */
#ifndef MULIB_MEMTABLE_H
#define MULIB_MEMTABLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_MEMTABLE_OK = 0,
    MULIB_MEMTABLE_NOTFOUND = 1,
    MULIB_MEMTABLE_EINVAL = -1,
    MULIB_MEMTABLE_ENOMEM = -2,
    MULIB_MEMTABLE_EOVERFLOW = -3
  };
  /** @brief Owned bytes; do not modify fields. Empty keys/values are valid. */
  typedef struct mulib_memtable_record
  {
    unsigned char *key, *value;
    size_t key_size, value_size;
    uint64_t sequence;
    int deleted;
  } mulib_memtable_record_t;
  /** @brief Initialize to zero; do not copy except through clone. Not
   * thread-safe. */
  typedef struct mulib_memtable
  {
    mulib_memtable_record_t *records;
    size_t count, capacity;
    uint64_t revision;
  } mulib_memtable_t;
  /** @brief Unsigned byte lexicographic comparison; NULL permitted for zero
   * size. */
  int mulib_memtable_compare (const void *a, size_t an, const void *b,
                              size_t bn);
  /** @brief First index whose key is >= key; invalid input returns SIZE_MAX.
   */
  size_t mulib_memtable_lower_bound (const mulib_memtable_t *t,
                                     const void *key, size_t n);
  /** @brief Borrow a record, including tombstones, until mutation/free. */
  int mulib_memtable_get (const mulib_memtable_t *t, const void *key, size_t n,
                          const mulib_memtable_record_t **out);
  /** @brief Copy/replace a record; older sequences are ignored, equal
   * replaces. deleted must be 0/1 and tombstones must have empty values.
   * Failure is atomic. Input bytes may alias existing records. Revision
   * advances only on mutation. */
  int mulib_memtable_put (mulib_memtable_t *t, const void *key, size_t kn,
                          const void *value, size_t vn, uint64_t sequence,
                          int deleted);
  /** @brief Deep copy into a zero/uninitialized destination; unchanged on
   * failure. */
  int mulib_memtable_clone (const mulib_memtable_t *src,
                            mulib_memtable_t *out);
  /** @brief Release and zero, accepting NULL. */
  void mulib_memtable_free (mulib_memtable_t *t);
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
| Header | [`include/mulib-memtable.h`](../include/mulib-memtable.h) |
| Implementation switch | `MULIB_IMPL_MEMTABLE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/memtable.c`](../examples/modules/memtable.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/memtable.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
