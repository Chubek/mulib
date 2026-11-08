# mulib-strpool

[Manuals](README.md) · [Library index](index.md)

Binary-safe, owning string interning with chained FNV-1a hash buckets. It
dogfoods `mulib-hash`, `mulib-alloc`, and `mulib-port`. Define implementation
switches for those modules and `MULIB_IMPL_STRPOOL` exactly once.

## API

`create(allocator)` copies callbacks and returns an owned pool. `intern(pool,s)`
accepts a C string; `intern_n(pool,s,n)` accepts bytes, including embedded NUL.
The latter permits NULL for an empty string. The returned borrowed pointer has
a convenience trailing NUL. Identical bytes and lengths return identical
pointers, even after bucket growth. Different lengths remain distinct.

`find_n` looks up without insertion. `reserve(pool,count)` ensures capacity
for count unique strings at a maximum 75% load. Expected lookup is O(n) for
hashing n bytes plus bucket search; FNV-1a is not collision-attack resistant.
All allocations and counts are overflow checked. Failure returns NULL/0 and
does not invalidate existing interned pointers.

`size` counts unique strings; `bytes` sums their lengths, excluding convenience
NULs and overhead. `reset` releases strings, invalidating all borrowed pointers
but retaining buckets. `destroy` releases the entire pool. No individual
deletion or reference counting; use separate pools for independent lifetimes.
Pools are not thread-safe.

## Example

```c
#define MULIB_IMPL_PORT
#define MULIB_IMPL_ALLOC
#define MULIB_IMPL_HASH
#define MULIB_IMPL_STRPOOL
#include <mulib-strpool.h>
#include <assert.h>
int main(void) {
    mulib_strpool_t *p = mulib_strpool_create(NULL);
    const char *a;
    assert(p);
    a = mulib_strpool_intern(p, "name");
    assert(a && a == mulib_strpool_intern(p, "name"));
    mulib_strpool_destroy(p);
    return 0;
}
```

Runnable example: `examples/modules/strpool.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-strpool.h`](../include/mulib-strpool.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-strpool.h
 * @brief Binary-safe string interning with stable addresses and hashed lookup.
 * Define MULIB_IMPL_STRPOOL, MULIB_IMPL_ALLOC, MULIB_IMPL_PORT,
 * MULIB_IMPL_HASH once. Interned pointers survive growth, not reset/destroy.
 */
#ifndef MULIB_STRPOOL_H
#define MULIB_STRPOOL_H
#include "mulib-alloc.h"
#include "mulib-hash.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_strpool mulib_strpool_t;
  /** @brief Create an initially empty pool; copy allocator callbacks. */
  mulib_strpool_t *mulib_strpool_create (const mulib_allocator_t *allocator);
  /** @brief Intern a NUL-terminated string, returning a pool-owned pointer. */
  const char *mulib_strpool_intern (mulib_strpool_t *pool, const char *text);
  /** @brief Intern length-delimited bytes, appending a convenience NUL. */
  const char *mulib_strpool_intern_n (mulib_strpool_t *pool, const char *text,
                                      size_t length);
  /** @brief Find without inserting; NULL means absent or invalid arguments. */
  const char *mulib_strpool_find_n (const mulib_strpool_t *pool,
                                    const char *text, size_t length);
  /** @brief Reserve space for at least count unique strings without rehashing.
   */
  int mulib_strpool_reserve (mulib_strpool_t *pool, size_t count);
  /** @brief Return unique string count. */
  size_t mulib_strpool_size (const mulib_strpool_t *pool);
  /** @brief Return summed unique byte lengths, excluding convenience NULs. */
  size_t mulib_strpool_bytes (const mulib_strpool_t *pool);
  /** @brief Release interned strings, retaining bucket capacity. */
  void mulib_strpool_reset (mulib_strpool_t *pool);
  /** @brief Release all resources. */
  void mulib_strpool_destroy (mulib_strpool_t *pool);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_STRPOOL_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-strpool.h`](../include/mulib-strpool.h) |
| Implementation switch | `MULIB_IMPL_STRPOOL` |
| Repository-local includes | [mulib-alloc](mulib-alloc.md), [mulib-hash](mulib-hash.md) |
| Complete example | [`examples/modules/strpool.c`](../examples/modules/strpool.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/strpool.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
