# browser-origin

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Normalized HTTP(S) origin parsing and same-origin checks.

## Contract and supported profile

Parse accepts absolute ASCII HTTP(S) URLs<=8192 bytes using MuLIB URI, with
no userinfo. Schemes/DNS hosts fold to lowercase, one DNS trailing dot is removed,
strict IPv4 is required for numeric hosts, IPv6 is parsed/canonically formatted,
and absent ports become 80/443. DNS labels are 1..63 alphanumeric/hyphen bytes,
without leading/trailing hyphen; numeric shorthand/hex IP spellings and invalid
ports are rejected. Output is unchanged on failure. Equal compares initialized
canonical records. No IDNA, percent-decoded hostnames, opaque/file origins or
public-suffix/site relationship is supplied.

## Integration and ownership

Include `include/mulib-browser-origin.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-origin.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-origin.h`](../include/mulib-browser-origin.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-origin.h
 * @brief Normalized HTTP(S) origin parsing and same-origin checks.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_ORIGIN in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_ORIGIN_H
#define MULIB_BROWSER_ORIGIN_H
#include "mulib-uri.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_origin
  {
    char scheme[8], host[256];
    uint16_t port;
  } mulib_browser_origin_t;
  /** @brief See manual/mulib-browser-origin.md for this operation's contract.
   */
  int mulib_browser_origin_parse (const char *url,
                                  mulib_browser_origin_t *out);
  /** @brief See manual/mulib-browser-origin.md for this operation's contract.
   */
  int mulib_browser_origin_equal (const mulib_browser_origin_t *a,
                                  const mulib_browser_origin_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_ORIGIN_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-origin.h`](../include/mulib-browser-origin.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_ORIGIN` |
| Repository-local includes | [mulib-uri](mulib-uri.md) |
| Complete example | [`examples/modules/browser-origin.c`](../examples/modules/browser-origin.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-origin.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
