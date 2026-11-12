# browser-snapshot

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Owned immutable byte snapshots with content fingerprint and document version.

## Contract and supported profile

Make creates an owned immutable-by-convention copy of arbitrary bytes,
with an opaque document version and FNV-1a content fingerprint. Optional max_bytes
is a payload limit; NULL/zero input is empty. Output must be fresh/released;
failure preserves it. Equal compares length, hash and actual bytes, deliberately
ignoring version (content equality across revisions). It is not a cryptographic
integrity check or serialized browser/DOM state. Free releases storage and resets
the record. Returned struct fields must not be mutated while treated as a snapshot.

## Integration and ownership

Include `include/mulib-browser-snapshot.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_BROWSER_SNAPSHOT`, `MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/browser-snapshot.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-browser-snapshot.h`](../include/mulib-browser-snapshot.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-browser-snapshot.h
 * @brief Owned immutable byte snapshots with content fingerprint and document
 * version. C11 single-header module. Define MULIB_IMPL_BROWSER_SNAPSHOT in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_BROWSER_SNAPSHOT_H
#define MULIB_BROWSER_SNAPSHOT_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_browser_snapshot
  {
    mulib_shylock_buffer_t bytes;
    uint64_t version, hash;
  } mulib_browser_snapshot_t;
  /** @brief See manual/mulib-browser-snapshot.md for this operation's
   * contract. */
  int mulib_browser_snapshot_make (mulib_browser_snapshot_t *out,
                                   const void *bytes, size_t len,
                                   uint64_t version, size_t max_bytes);
  /** @brief See manual/mulib-browser-snapshot.md for this operation's
   * contract. */
  int mulib_browser_snapshot_equal (const mulib_browser_snapshot_t *a,
                                    const mulib_browser_snapshot_t *b);
  /** @brief See manual/mulib-browser-snapshot.md for this operation's
   * contract. */
  void mulib_browser_snapshot_free (mulib_browser_snapshot_t *s);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_BROWSER_SNAPSHOT_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-browser-snapshot.h`](../include/mulib-browser-snapshot.h) |
| Implementation switch | `MULIB_IMPL_BROWSER_SNAPSHOT` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/browser-snapshot.c`](../examples/modules/browser-snapshot.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/browser-snapshot.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
