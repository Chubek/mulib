# websearch-engine

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Deduplicated bounded ranked web search result collection.

## Contract and supported profile

The zero-initialized collection copies validated HTTP(S) URLs and titles,
bounded by their fixed arrays. Deduplication is exact URL string equality;
duplicates keep the highest finite nonnegative score, OR provider masks, and
retain the original title even at capacity. Sort is descending score with URL
lexical tie breaking. Add and sort invalidate array views. Zero limit is unlimited;
free releases and resets. This is a result collection, not a network provider.

## Integration and ownership

Include `include/mulib-websearch-engine.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_WEBSEARCH_ENGINE`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/websearch-engine.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-websearch-engine.h`](../include/mulib-websearch-engine.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-websearch-engine.h
 * @brief Deduplicated bounded ranked web search result collection.
 * C11 single-header module. Define MULIB_IMPL_WEBSEARCH_ENGINE in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_WEBSEARCH_ENGINE_H
#define MULIB_WEBSEARCH_ENGINE_H
#include "mulib-browser-origin.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_websearch_hit
  {
    char url[1024], title[256];
    double score;
    uint32_t providers;
  } mulib_websearch_hit_t;
  typedef struct mulib_websearch_engine
  {
    mulib_websearch_hit_t *hits;
    size_t len, cap, limit;
  } mulib_websearch_engine_t;
  /** @brief See manual/mulib-websearch-engine.md for this operation's
   * contract. */
  int mulib_websearch_engine_add (mulib_websearch_engine_t *e, const char *url,
                                  const char *title, double score,
                                  uint32_t provider_flag);
  /** @brief See manual/mulib-websearch-engine.md for this operation's
   * contract. */
  void mulib_websearch_engine_sort (mulib_websearch_engine_t *e);
  /** @brief See manual/mulib-websearch-engine.md for this operation's
   * contract. */
  void mulib_websearch_engine_free (mulib_websearch_engine_t *e);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBSEARCH_ENGINE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-websearch-engine.h`](../include/mulib-websearch-engine.h) |
| Implementation switch | `MULIB_IMPL_WEBSEARCH_ENGINE` |
| Repository-local includes | [mulib-browser-origin](mulib-browser-origin.md), [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/websearch-engine.c`](../examples/modules/websearch-engine.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/websearch-engine.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
