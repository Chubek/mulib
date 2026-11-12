# crawlarchive

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Owned replayable crawl records with content hashes and bounded payloads.

## Contract and supported profile

Archive owns copied binary bodies and fixed-capacity URL strings, with an
optional per-record max_body budget. Status is 100..599. Repeated URLs append
records; find returns the newest exact match as a borrowed view, invalidated by
later growth/free. Hash is FNV-1a, useful for replay comparison, not authenticated
integrity. Zero limits mean unlimited; no serialization, disk persistence,
record-count limit or HTTP request execution is supplied. Release with free.

## Integration and ownership

Include `include/mulib-crawlarchive.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_CRAWLARCHIVE`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/crawlarchive.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-crawlarchive.h`](../include/mulib-crawlarchive.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-crawlarchive.h
 * @brief Owned replayable crawl records with content hashes and bounded
 * payloads. C11 single-header module. Define MULIB_IMPL_CRAWLARCHIVE in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_CRAWLARCHIVE_H
#define MULIB_CRAWLARCHIVE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_crawlarchive_record
  {
    char url[1024];
    unsigned status;
    uint64_t hash;
    mulib_shylock_buffer_t body;
  } mulib_crawlarchive_record_t;
  typedef struct mulib_crawlarchive
  {
    mulib_crawlarchive_record_t *items;
    size_t len, cap, max_body;
  } mulib_crawlarchive_t;
  /** @brief See manual/mulib-crawlarchive.md for this operation's contract. */
  int mulib_crawlarchive_add (mulib_crawlarchive_t *a, const char *url,
                              unsigned status, const void *body,
                              size_t length);
  const mulib_crawlarchive_record_t *
  /** @brief See manual/mulib-crawlarchive.md for this operation's contract. */
  mulib_crawlarchive_find (const mulib_crawlarchive_t *a, const char *url);
  /** @brief See manual/mulib-crawlarchive.md for this operation's contract. */
  void mulib_crawlarchive_free (mulib_crawlarchive_t *a);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CRAWLARCHIVE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-crawlarchive.h`](../include/mulib-crawlarchive.h) |
| Implementation switch | `MULIB_IMPL_CRAWLARCHIVE` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/crawlarchive.c`](../examples/modules/crawlarchive.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/crawlarchive.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
