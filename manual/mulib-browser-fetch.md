# browser-fetch

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Browser fetch request adapter with origin and security policy checks.

## Contract and supported profile

Get validates a target HTTP(S) origin, optionally compares it against the
canonical initiator, then delegates to fetch's policy/transport and atomic
response behavior. The struct borrows the transport context; initialize it to
zero and configure policy/transport explicitly. Cross-origin enabled requests
still require outbound policy checks. There is no CORS, preflight, credential,
referrer, mixed-content or Fetch Standard implementation. Origin checks do not
replace resolved-address and redirect enforcement in the transport.

## Integration and ownership

Include `include/mulib-browser-fetch.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_FETCH`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_FETCH`, `MULIB_IMPL_FETCH_POLICY`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-fetch.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-fetch.h`](../include/mulib-browser-fetch.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-fetch.h
 * @brief Browser fetch request adapter with origin and security policy checks.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_FETCH in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_FETCH_H
#define MULIB_BROWSER_FETCH_H
#include "mulib-browser-origin.h"
#include "mulib-fetch.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_fetch
  {
    mulib_fetch_client_t client;
    mulib_browser_origin_t initiator;
    int enforce_same_origin;
  } mulib_browser_fetch_t;
  /** @brief See manual/mulib-browser-fetch.md for this operation's contract.
   */
  int mulib_browser_fetch_get (const mulib_browser_fetch_t *f, const char *url,
                               mulib_shylock_buffer_t *body, unsigned *status,
                               char *content_type, size_t cap);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_FETCH_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-fetch.h`](../include/mulib-browser-fetch.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_FETCH` |
| Repository-local includes | [mulib-browser-origin](mulib-browser-origin.md), [mulib-fetch](mulib-fetch.md) |
| Complete example | [`examples/modules/browser-fetch.c`](../examples/modules/browser-fetch.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-fetch.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
