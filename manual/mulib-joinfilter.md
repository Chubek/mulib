# joinfilter — Bloom prefilter

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_JOINFILTER`.

Init zero state with a positive bit count and seed, returning 1 or 0 on
invalid/overflow/allocation failure. Four deterministic probes per key are used.
Add/maybe accept binary keys, including NULL/zero-length empty keys. Invalid
add is ignored; invalid maybe returns 0. Inserted keys never return false after
insertion; positive results require exact verification because collisions exist.
There is no deletion or merge API. Hashing is noncryptographic. Storage is
ceil(bits/8), query/add cost O(key bytes), and free resets state for reuse.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-joinfilter.h`](../include/mulib-joinfilter.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Bounded Bloom join filter with no false negatives for inserted keys. C11
 * single-header; define MULIB_IMPL_JOINFILTER once. */
#ifndef MULIB_JOINFILTER_H
#define MULIB_JOINFILTER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    unsigned char *bits;
    size_t bit_count;
    uint64_t seed;
  } mulib_joinfilter_t;
  int mulib_joinfilter_init (mulib_joinfilter_t *f, size_t bit_count, uint64_t seed);
  void mulib_joinfilter_add (mulib_joinfilter_t *f, const void *key, size_t length);
  int mulib_joinfilter_maybe (const mulib_joinfilter_t *f, const void *key,
                              size_t length);
  void mulib_joinfilter_free (mulib_joinfilter_t *f);
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
| Header | [`include/mulib-joinfilter.h`](../include/mulib-joinfilter.h) |
| Implementation switch | `MULIB_IMPL_JOINFILTER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/joinfilter.c`](../examples/modules/joinfilter.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/joinfilter.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
