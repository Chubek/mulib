# browser-permissions

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Explicit bitmask permissions for browser capabilities.

## Contract and supported profile

Permissions are explicit copied bitmasks for network/storage/script/cross-
origin/file-URL capabilities. Grant rejects unknown bits without mutation; check
requires all requested bits and rejects unknown bits in either mask (zero
requirements pass for a valid mask). Revoke clears requested bits and tolerates
NULL. Zero-initialized state grants nothing. These predicates do not intercept
operations: hosts must call them before protected actions. No ambient authority,
OS sandbox, user prompt or automatic browser-engine integration is implied.

## Integration and ownership

Include `include/mulib-browser-permissions.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_PERMISSIONS`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-permissions.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-permissions.h`](../include/mulib-browser-permissions.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-permissions.h
 * @brief Explicit bitmask permissions for browser capabilities.
 * C11 single-header module. Define MULIB_IMPL_BROWSER_PERMISSIONS in exactly
 * one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_PERMISSIONS_H
#define MULIB_BROWSER_PERMISSIONS_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_browser_permission
  {
    MULIB_BROWSER_ALLOW_NETWORK = 1u,
    MULIB_BROWSER_ALLOW_STORAGE = 2u,
    MULIB_BROWSER_ALLOW_SCRIPT = 4u,
    MULIB_BROWSER_ALLOW_CROSS_ORIGIN = 8u,
    MULIB_BROWSER_ALLOW_FILE_URL = 16u
  } mulib_browser_permission_t;
  typedef struct mulib_browser_permissions
  {
    uint32_t allowed;
  } mulib_browser_permissions_t;
  /** @brief See manual/mulib-browser-permissions.md for this operation's
   * contract. */
  int mulib_browser_permissions_check (mulib_browser_permissions_t permissions,
                                       uint32_t required);
  int
  /** @brief See manual/mulib-browser-permissions.md for this operation's
     contract. */
  mulib_browser_permissions_grant (mulib_browser_permissions_t *permissions,
                                   uint32_t bits);
  void
  /** @brief See manual/mulib-browser-permissions.md for this operation's
     contract. */
  mulib_browser_permissions_revoke (mulib_browser_permissions_t *permissions,
                                    uint32_t bits);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_PERMISSIONS_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-permissions.h`](../include/mulib-browser-permissions.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_PERMISSIONS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/browser-permissions.c`](../examples/modules/browser-permissions.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-permissions.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
