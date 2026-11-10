# postings — sorted document frequencies

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_POSTINGS`.

Put inserts a uint64 document ID with positive uint32 frequency or replaces the
existing frequency (it does not accumulate). Return 1 on success, 0 for invalid
frequency/pointer or allocation/size failure, preserving state on failure.
Find returns 1 if present and optionally writes frequency; absence returns 0
without writing. Remove returns 1 if removed, 0 if absent/invalid. IDs include
zero and UINT64_MAX. Lookup is O(log n), insertion/removal O(n); growth can
invalidate borrowed array pointers. Free releases and zeros the owner.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-postings.h`](../include/mulib-postings.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Sorted document postings with term frequency, binary search and set-safe
 * updates. C11 single-header; define MULIB_IMPL_POSTINGS once. */
#ifndef MULIB_POSTINGS_H
#define MULIB_POSTINGS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t document;
    uint32_t frequency;
  } mulib_posting_t;
  typedef struct
  {
    mulib_posting_t *items;
    size_t count, capacity;
  } mulib_postings_t;
  int mulib_postings_put (mulib_postings_t *p, uint64_t document, uint32_t frequency);
  int mulib_postings_find (const mulib_postings_t *p, uint64_t document,
                           uint32_t *frequency);
  int mulib_postings_remove (mulib_postings_t *p, uint64_t document);
  void mulib_postings_free (mulib_postings_t *p);
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
| Header | [`include/mulib-postings.h`](../include/mulib-postings.h) |
| Implementation switch | `MULIB_IMPL_POSTINGS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/postings.c`](../examples/modules/postings.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/postings.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
