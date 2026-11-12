# browser-navigation

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Owned bounded navigation history and back/forward traversal.

## Contract and supported profile

History copies nonempty URL identifiers<=8192 bytes. Visit discards forward
entries only after allocations succeed and appends the new current URL; input
may borrow an existing URL. The optional count limit applies after forward
truncation; a full current history rejects rather than evicts. Move accepts a
signed relative delta, returns the borrowed current URL, and preserves position
when out of range. URLs are not validated or fetched here. Free resets history;
visits may invalidate URL views that were in the forward branch.

## Integration and ownership

Include `include/mulib-browser-navigation.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_NAVIGATION`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-navigation.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-navigation.h`](../include/mulib-browser-navigation.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-navigation.h
 * @brief Owned bounded navigation history and back/forward traversal.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_NAVIGATION in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_NAVIGATION_H
#define MULIB_BROWSER_NAVIGATION_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_navigation
  {
    char **urls;
    size_t len, cap, current, limit;
  } mulib_browser_navigation_t;
  /** @brief See manual/mulib-browser-navigation.md for this operation's
   * contract. */
  int mulib_browser_navigation_visit (mulib_browser_navigation_t *n,
                                      const char *url);
  /** @brief See manual/mulib-browser-navigation.md for this operation's
   * contract. */
  const char *mulib_browser_navigation_move (mulib_browser_navigation_t *n,
                                             int delta);
  /** @brief See manual/mulib-browser-navigation.md for this operation's
   * contract. */
  void mulib_browser_navigation_free (mulib_browser_navigation_t *n);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_NAVIGATION_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-navigation.h`](../include/mulib-browser-navigation.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_NAVIGATION` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/browser-navigation.c`](../examples/modules/browser-navigation.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-navigation.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
