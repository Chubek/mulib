# browser-resource

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Deduplicated resource registry with load state and content type.

## Contract and supported profile

A zero-initialized resource registry copies nonempty URLs up to 1023 bytes;
URLs here are identifiers, not validated fetch targets. Register deduplicates
exact strings and returns a stable numeric index, honoring optional count limit.
Complete is one-shot, requires status 100..599 and a fitting content type without
CR/LF, and copies status/byte count/loaded flag atomically. Views expire on
array growth/free. Free resets storage and limits; no network activity is performed.

## Integration and ownership

Include `include/mulib-browser-resource.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_RESOURCE`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-resource.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-resource.h`](../include/mulib-browser-resource.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-resource.h
 * @brief Deduplicated resource registry with load state and content type.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_RESOURCE in exactly one
 * C translation unit.
 *
 */
#ifndef MULIB_BROWSER_RESOURCE_H
#define MULIB_BROWSER_RESOURCE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_resource
  {
    char url[1024], content_type[128];
    unsigned status;
    uint64_t bytes;
    int loaded;
  } mulib_browser_resource_t;
  typedef struct mulib_browser_resources
  {
    mulib_browser_resource_t *items;
    size_t len, cap, limit;
  } mulib_browser_resources_t;
  /** @brief See manual/mulib-browser-resource.md for this operation's
   * contract. */
  int mulib_browser_resource_register (mulib_browser_resources_t *r,
                                       const char *url, size_t *id);
  /** @brief See manual/mulib-browser-resource.md for this operation's
   * contract. */
  int mulib_browser_resource_complete (mulib_browser_resources_t *r, size_t id,
                                       unsigned status, const char *type,
                                       uint64_t bytes);
  /** @brief See manual/mulib-browser-resource.md for this operation's
   * contract. */
  void mulib_browser_resources_free (mulib_browser_resources_t *r);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_RESOURCE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-resource.h`](../include/mulib-browser-resource.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_RESOURCE` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/browser-resource.c`](../examples/modules/browser-resource.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-resource.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
