# crawler

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Bounded crawl frontier with retry scheduling and URL deduplication.

## Contract and supported profile

Enqueue copies validated HTTP(S) URL strings and nonnegative due times in
caller-defined milliseconds. Exact URL duplicates are accepted without adding
another job, including completed jobs. `next` chooses the earliest due queued
job, ties by insertion order, marks it active and increments checked attempts.
`finish` requires an active ID and positive attempt limit: success/exhaustion
marks done; otherwise retry_at must not precede the prior due time. Invalid
retries preserve state. No I/O, robots policy, persistence or concurrent scheduling
is supplied. IDs are stable until free; array views are not.

## Integration and ownership

Include `include/mulib-crawler.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_CRAWLER`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/crawler.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-crawler.h`](../include/mulib-crawler.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-crawler.h
 * @brief Bounded crawl frontier with retry scheduling and URL deduplication.
 * C11 single-header module. Define MULIB_IMPL_CRAWLER in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_CRAWLER_H
#define MULIB_CRAWLER_H
#include "mulib-browser-origin.h"
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_crawler_job
  {
    char url[1024];
    int64_t due_ms;
    unsigned attempts;
    uint8_t state;
  } mulib_crawler_job_t;
  typedef struct mulib_crawler
  {
    mulib_crawler_job_t *items;
    size_t len, cap, limit;
  } mulib_crawler_t;
  /** @brief See manual/mulib-crawler.md for this operation's contract. */
  int mulib_crawler_enqueue (mulib_crawler_t *c, const char *url, int64_t due);
  /** @brief See manual/mulib-crawler.md for this operation's contract. */
  int mulib_crawler_next (mulib_crawler_t *c, int64_t now, size_t *id);
  /** @brief See manual/mulib-crawler.md for this operation's contract. */
  int mulib_crawler_finish (mulib_crawler_t *c, size_t id, int success,
                            int64_t retry_at, unsigned max_attempts);
  /** @brief See manual/mulib-crawler.md for this operation's contract. */
  void mulib_crawler_free (mulib_crawler_t *c);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CRAWLER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-crawler.h`](../include/mulib-crawler.h) |
| Implementation switch | `MULIB_IMPL_CRAWLER` |
| Repository-local includes | [mulib-browser-origin](mulib-browser-origin.md), [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/crawler.c`](../examples/modules/crawler.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/crawler.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
