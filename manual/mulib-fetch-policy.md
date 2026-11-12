# fetch-policy

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Conservative outbound URL security checks and restricted fetch policy.

## Contract and supported profile

Check applies URL length (default 8192 bytes), strict normalized HTTP(S)
origin syntax and enabled schemes. Private access is denied by default, including
localhost/local suffixes, loopback/private/link-local/shared IPv4 ranges, ambiguous
numeric addresses and all IPv6 literals (conservative fail-closed classification).
allow_private relaxes address classification, not URL syntax. max_bytes is a fetch
budget, not checked by this predicate. Hostnames are not resolved: DNS rebinding,
redirects, proxies and actual peer-address checks remain transport obligations.
This predicate alone is NOT an SSRF boundary.

The conservative hostname classifier also rejects digit-prefixed DNS names;
use a transport's resolved-peer authorization rather than weakening URL syntax
to work around a rejected target.

## Integration and ownership

Include `include/mulib-fetch-policy.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_FETCH_POLICY`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/fetch-policy.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-fetch-policy.h`](../include/mulib-fetch-policy.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-fetch-policy.h
 * @brief Conservative outbound URL security checks and restricted fetch
 * policy. C11 single-header module. Define MULIB_IMPL_FETCH_POLICY in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_FETCH_POLICY_H
#define MULIB_FETCH_POLICY_H
#include "mulib-browser-origin.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_fetch_policy
  {
    size_t max_url, max_bytes;
    int allow_http, allow_https, allow_private;
  } mulib_fetch_policy_t;
  /** @brief See manual/mulib-fetch-policy.md for this operation's contract. */
  int mulib_fetch_policy_check (const mulib_fetch_policy_t *p,
                                const char *url);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_FETCH_POLICY_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fetch-policy.h`](../include/mulib-fetch-policy.h) |
| Implementation switch | `MULIB_IMPL_FETCH_POLICY` |
| Repository-local includes | [mulib-browser-origin](mulib-browser-origin.md) |
| Complete example | [`examples/modules/fetch-policy.c`](../examples/modules/fetch-policy.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/fetch-policy.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
