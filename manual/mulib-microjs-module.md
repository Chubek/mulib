# microjs-module

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Owned source-module registry with immutable lookup by specifier.

## Contract and supported profile

The zero-initialized registry copies nonempty specifiers (<=512 bytes) and
UTF-8 source bytes (<=8 MiB); NULL/zero source is empty. Exact duplicate names
are rejected; no replacement occurs. An optional count limit is enforced.
Find borrows a const record; record pointers expire on growth/free, while copied
source is owned until free. Source length is authoritative and may include NUL.
No imports, URL resolution, evaluation, dependency cycles or module caching
semantics beyond immutable registration are implemented.

## Integration and ownership

Include `include/mulib-microjs-module.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_MICROJS_MODULE`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/microjs-module.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-microjs-module.h`](../include/mulib-microjs-module.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-microjs-module.h
 * @brief Owned source-module registry with immutable lookup by specifier.
 * C11 single-header module. Define MULIB_IMPL_MICROJS_MODULE in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_MICROJS_MODULE_H
#define MULIB_MICROJS_MODULE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_microjs_module
  {
    char *name, *source;
    size_t source_length;
  } mulib_microjs_module_t;
  typedef struct mulib_microjs_modules
  {
    mulib_microjs_module_t *items;
    size_t len, cap, limit;
  } mulib_microjs_modules_t;
  /** @brief See manual/mulib-microjs-module.md for this operation's contract.
   */
  int mulib_microjs_module_register (mulib_microjs_modules_t *m,
                                     const char *name, const char *source,
                                     size_t length);
  const mulib_microjs_module_t *
  /** @brief See manual/mulib-microjs-module.md for this operation's contract.
   */
  mulib_microjs_module_find (const mulib_microjs_modules_t *m,
                             const char *name);
  /** @brief See manual/mulib-microjs-module.md for this operation's contract.
   */
  void mulib_microjs_modules_free (mulib_microjs_modules_t *m);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_MICROJS_MODULE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-microjs-module.h`](../include/mulib-microjs-module.h) |
| Implementation switch | `MULIB_IMPL_MICROJS_MODULE` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/microjs-module.c`](../examples/modules/microjs-module.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/microjs-module.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
