# browser-cookiejar

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Host/domain/path/secure-aware in-memory browser cookie storage.

## Contract and supported profile

Put copies pre-parsed cookies, rejects unsafe header name/value bytes,
requires an absolute safe path/nonnegative expiry and normalizes ASCII domain
case. A leading domain dot is not accepted. Exact name/domain/path replaces even
at count limit. Header appends matching unexpired cookies in insertion order,
with host-only/domain-boundary matching, secure filtering and path boundaries
(`/a` matches `/a` or `/a/b`, not `/ab`). Expiry zero means session cookie.
Output is staged. The host must authorize Domain against the source host/public
suffixes: no Set-Cookie parser, SameSite, partitioning, script HttpOnly enforcement
or public-suffix database is included. This is not a complete browser cookie jar.

## Integration and ownership

Include `include/mulib-browser-cookiejar.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_COOKIEJAR`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-cookiejar.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-cookiejar.h`](../include/mulib-browser-cookiejar.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-cookiejar.h
 * @brief Host/domain/path/secure-aware in-memory browser cookie storage.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_COOKIEJAR in exactly one
 * C translation unit. A bounded host/domain/path profile, not full RFC6265bis
 * or SameSite semantics.
 */
#ifndef MULIB_BROWSER_COOKIEJAR_H
#define MULIB_BROWSER_COOKIEJAR_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_cookie
  {
    char name[128], value[512], domain[256], path[256];
    int64_t expiry_unix;
    unsigned secure, host_only, http_only;
  } mulib_browser_cookie_t;
  typedef struct mulib_browser_cookiejar
  {
    mulib_browser_cookie_t *items;
    size_t len, cap, limit;
  } mulib_browser_cookiejar_t;
  /** @brief See manual/mulib-browser-cookiejar.md for this operation's
   * contract. */
  int mulib_browser_cookiejar_put (mulib_browser_cookiejar_t *j,
                                   const mulib_browser_cookie_t *cookie);
  /** @brief See manual/mulib-browser-cookiejar.md for this operation's
   * contract. */
  int mulib_browser_cookiejar_header (const mulib_browser_cookiejar_t *j,
                                      const char *hostname, const char *path,
                                      int tls, int64_t now,
                                      mulib_shylock_buffer_t *out);
  /** @brief See manual/mulib-browser-cookiejar.md for this operation's
   * contract. */
  void mulib_browser_cookiejar_free (mulib_browser_cookiejar_t *j);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_COOKIEJAR_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-cookiejar.h`](../include/mulib-browser-cookiejar.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_COOKIEJAR` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/browser-cookiejar.c`](../examples/modules/browser-cookiejar.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-cookiejar.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
