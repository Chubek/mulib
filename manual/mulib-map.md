# MuLIB map

[Manuals](README.md) · [Library index](index.md)

String-keyed dictionary with owning keys and borrowed values. This is a compact linear-search dictionary, not a hash map. Use for modest configuration tables, not large high-throughput indexes.

## Contract and behavior

Initialize with `{0}` or `init()`. Keys are copied NUL-terminated strings;
values are borrowed `void *` pointers, including NULL. Replacing/removing an
entry never destroys its value. Set returns 1 on success, 0 on allocation or
argument failure. Get returns 1 if present and optionally writes the value,
so a stored NULL can be distinguished from an absent key. Missing/invalid get
leaves output unchanged. Remove returns 1 only for an existing key.

Lookup is linear; this is an array map, not a hash table. Removal shifts entries.
`free()` releases keys and entry storage and zeros state. Free live state before
calling init again; callers retain responsibility for value lifetimes.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_MAP
#include "mulib-map.h"
int main(void) {
    mulib_map_t m;
    mulib_map_init(&m);
    int x = 3;
    assert(mulib_map_set(&m, "key", &x));
    void *p = NULL;
    assert(mulib_map_get(&m, "key", &p) && p == &x);
    mulib_map_free(&m);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-map.h`](../include/mulib-map.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-map.h
 * @brief String-keyed dictionary with owning keys and borrowed values.
 * @defgroup mulib_map MuLIB map
 * @{
 * @details This is a compact linear-search dictionary, not a hash map. Use for
 * modest configuration tables, not large high-throughput indexes.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MAP before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_MAP_H
#define MULIB_MAP_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief String-keyed dictionary with owning keys and borrowed values. */
  typedef struct mulib_map_entry
  {
    char *key;
    void *value;
  } mulib_map_entry_t;
  /** @brief String-keyed dictionary with owning keys and borrowed values. */
  typedef struct mulib_map
  {
    mulib_map_entry_t *items;
    size_t length, capacity;
  } mulib_map_t;
  /** @brief mulib map init; see manual/mulib-map.md for contracts and
   * examples. */
  void mulib_map_init (mulib_map_t *m);
  /** @brief mulib map set; see manual/mulib-map.md for contracts and examples.
   */
  int mulib_map_set (mulib_map_t *m, const char *key, void *value);
  /** @brief mulib map get; see manual/mulib-map.md for contracts and examples.
   */
  int mulib_map_get (const mulib_map_t *m, const char *key, void **out);
  /** @brief mulib map remove; see manual/mulib-map.md for contracts and
   * examples. */
  int mulib_map_remove (mulib_map_t *m, const char *key);
  /** @brief mulib map free; see manual/mulib-map.md for contracts and
   * examples. */
  void mulib_map_free (mulib_map_t *m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MAP_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-map.h`](../include/mulib-map.h) |
| Implementation switch | `MULIB_IMPL_MAP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/map.c`](../examples/modules/map.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/map.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
