# browser-realm

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Browser realm IDs with immutable origin metadata and isolation keys.

## Contract and supported profile

Init copies nonzero realm/context IDs and canonical HTTP(S) origin metadata,
serializing normalized host and explicit port (IPv6 bracketed). Flags are opaque
host metadata. Invalid or oversized origin leaves output unchanged. Same-origin
compares canonical origin strings of initialized records, not context IDs;
context isolation decisions must check context_id separately. These are value
records, not VM realms, opaque origins, process isolation or script execution.

## Integration and ownership

Include `include/mulib-browser-realm.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_AST`, `MULIB_IMPL_BROWSER_ORIGIN`, `MULIB_IMPL_BROWSER_REALM`, `MULIB_IMPL_GLOB`, `MULIB_IMPL_INET4`, `MULIB_IMPL_INET6`, `MULIB_IMPL_INTE6`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PEG`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_URI`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-realm.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-realm.h`](../include/mulib-browser-realm.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-realm.h
 * @brief Browser realm IDs with immutable origin metadata and isolation keys.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_REALM in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_BROWSER_REALM_H
#define MULIB_BROWSER_REALM_H
#include "mulib-browser-origin.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_realm
  {
    uint64_t id, context_id;
    char origin[256];
    uint32_t flags;
  } mulib_browser_realm_t;
  /** @brief See manual/mulib-browser-realm.md for this operation's contract.
   */
  int mulib_browser_realm_init (mulib_browser_realm_t *out, uint64_t id,
                                uint64_t context, const char *origin,
                                unsigned flags);
  /** @brief See manual/mulib-browser-realm.md for this operation's contract.
   */
  int mulib_browser_realm_same_origin (const mulib_browser_realm_t *a,
                                       const mulib_browser_realm_t *b);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_REALM_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-realm.h`](../include/mulib-browser-realm.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_REALM` |
| Repository-local includes | [mulib-browser-origin](mulib-browser-origin.md) |
| Complete example | [`examples/modules/browser-realm.c`](../examples/modules/browser-realm.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-realm.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
