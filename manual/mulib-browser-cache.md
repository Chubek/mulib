# browser-cache

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Aggregate-byte-bounded URL response cache with expiry pruning and ETag metadata.

## Contract and supported profile

Cache copies URL/ETag/body and uses absolute nonnegative expiry in caller
time units. Get returns a borrowed exact URL match only when now<expires; prune
removes expired records and returns their count. Put replaces matching URLs
atomically; max_entries and max_bytes bound records and TOTAL retained payload
bytes (zero unlimited). There is no automatic eviction; expired entries count
until pruned. ETag is stored metadata for caller comparison, not automatic HTTP
revalidation. Borrowed views expire on replacement, growth, pruning or free.
There is no Vary, Cache-Control, credential partitioning or HTTP cache conformance.

## Integration and ownership

Include `include/mulib-browser-cache.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_CACHE`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-cache.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-cache.h`](../include/mulib-browser-cache.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-cache.h
 * @brief Aggregate-byte-bounded response cache with expiry and ETag metadata.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_CACHE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_CACHE_H
#define MULIB_BROWSER_CACHE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_cache_entry
  {
    char url[1024], etag[256];
    int64_t expires;
    mulib_shylock_buffer_t body;
  } mulib_browser_cache_entry_t;
  typedef struct mulib_browser_cache
  {
    mulib_browser_cache_entry_t *items;
    size_t len, cap, max_entries, max_bytes;
  } mulib_browser_cache_t;
  /** @brief See manual/mulib-browser-cache.md for this operation's contract.
   */
  int mulib_browser_cache_put (mulib_browser_cache_t *c, const char *url,
                               const char *etag, int64_t expiry,
                               const void *data, size_t n);
  const mulib_browser_cache_entry_t *
  /** @brief See manual/mulib-browser-cache.md for this operation's contract.
   */
  mulib_browser_cache_get (const mulib_browser_cache_t *c, const char *url,
                           int64_t now);
  /** @brief Remove expired entries and return their count; borrowed views
   * expire. */
  size_t mulib_browser_cache_prune (mulib_browser_cache_t *cache, int64_t now);
  /** @brief See manual/mulib-browser-cache.md for this operation's contract.
   */
  void mulib_browser_cache_free (mulib_browser_cache_t *c);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_CACHE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-cache.h`](../include/mulib-browser-cache.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_CACHE` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/browser-cache.c`](../examples/modules/browser-cache.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-cache.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
